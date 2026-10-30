#include "private/applicationCatalogPrivate.h"

#include "platform/windows/windowsText.h"

#include <Windows.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <unordered_set>

namespace desktopctl {

ApplicationCatalogPrivate::ApplicationCatalogPrivate()
{
    const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (result == S_OK || result == S_FALSE) {
        comAvailable_ = true;
        uninitializeCom_ = true;
    } else if (result == RPC_E_CHANGED_MODE) {
        // COM is already initialized with another apartment model and remains usable here.
        comAvailable_ = true;
    }
}

ApplicationCatalogPrivate::~ApplicationCatalogPrivate()
{
    if (uninitializeCom_) {
        CoUninitialize();
    }
}

std::string ApplicationCatalogPrivate::lowerAscii(const std::string& value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

std::wstring ApplicationCatalogPrivate::shellItemName(PCIDLIST_ABSOLUTE item, SIGDN form)
{
    PWSTR rawName = nullptr;
    if (FAILED(SHGetNameFromIDList(item, form, &rawName)) || rawName == nullptr) {
        return {};
    }
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> name(rawName, CoTaskMemFree);
    return name.get();
}

std::wstring ApplicationCatalogPrivate::applicationUserModelId(PCIDLIST_ABSOLUTE item)
{
    Microsoft::WRL::ComPtr<IPropertyStore> properties;
    if (FAILED(SHGetPropertyStoreFromIDList(item, GPS_DEFAULT, IID_PPV_ARGS(&properties)))) {
        return {};
    }

    PROPVARIANT value{};
    PropVariantInit(&value);
    if (FAILED(properties->GetValue(PKEY_AppUserModel_ID, &value))) {
        PropVariantClear(&value);
        return {};
    }
    wchar_t buffer[1024]{};
    const HRESULT conversion = PropVariantToString(value, buffer,
                                                    static_cast<UINT>(std::size(buffer)));
    PropVariantClear(&value);
    return SUCCEEDED(conversion) ? std::wstring(buffer) : std::wstring();
}

OperationResult ApplicationCatalogPrivate::discover(
    std::vector<ApplicationRecord>& applications)
{
    applications.clear();
    if (!comAvailable_) {
        return {false, "COM initialization failed; the Windows application catalog is unavailable."};
    }

    Microsoft::WRL::ComPtr<IShellFolder> desktopFolder;
    HRESULT result = SHGetDesktopFolder(&desktopFolder);
    if (FAILED(result)) {
        return {false, "Unable to access the Windows desktop Shell folder."};
    }

    PIDLIST_ABSOLUTE rawAppsFolder = nullptr;
    result = SHParseDisplayName(L"shell:AppsFolder", nullptr, &rawAppsFolder, 0, nullptr);
    if (FAILED(result) || rawAppsFolder == nullptr) {
        return {false, "Unable to resolve the Windows AppsFolder."};
    }
    const std::unique_ptr<void, decltype(&CoTaskMemFree)> appsFolderMemory(rawAppsFolder,
                                                                          CoTaskMemFree);

    Microsoft::WRL::ComPtr<IShellFolder> appsFolder;
    result = desktopFolder->BindToObject(rawAppsFolder, nullptr, IID_PPV_ARGS(&appsFolder));
    if (FAILED(result)) {
        return {false, "Unable to open the Windows AppsFolder."};
    }

    Microsoft::WRL::ComPtr<IEnumIDList> enumerator;
    result = appsFolder->EnumObjects(nullptr, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS,
                                     &enumerator);
    if (FAILED(result)) {
        return {false, "Unable to enumerate the Windows AppsFolder."};
    }

    for (;;) {
        PITEMID_CHILD rawChild = nullptr;
        ULONG fetched = 0;
        result = enumerator->Next(1, &rawChild, &fetched);
        if (result == S_FALSE || fetched == 0) {
            break;
        }
        if (FAILED(result) || rawChild == nullptr) {
            return {false, "Windows AppsFolder enumeration failed."};
        }
        const std::unique_ptr<void, decltype(&CoTaskMemFree)> childMemory(rawChild,
                                                                          CoTaskMemFree);
        PIDLIST_ABSOLUTE rawAbsolute = ILCombine(rawAppsFolder, rawChild);
        if (rawAbsolute == nullptr) {
            continue;
        }
        const std::unique_ptr<void, decltype(&CoTaskMemFree)> absoluteMemory(rawAbsolute,
                                                                             CoTaskMemFree);

        const std::wstring displayName = shellItemName(rawAbsolute, SIGDN_NORMALDISPLAY);
        const std::wstring parsingName = shellItemName(rawAbsolute,
                                                       SIGDN_DESKTOPABSOLUTEPARSING);
        if (displayName.empty() || parsingName.empty()) {
            continue;
        }

        const std::wstring modelId = applicationUserModelId(rawAbsolute);
        ApplicationRecord record;
        record.info.name = WindowsText::utf8FromWide(displayName);
        record.info.id = modelId.empty() ? "shell:" + WindowsText::utf8FromWide(parsingName)
                                         : "app:" + WindowsText::utf8FromWide(modelId);
        record.applicationUserModelId = modelId;
        record.launchTarget = parsingName;
        if (!record.info.name.empty() && !record.info.id.empty()) {
            applications.push_back(std::move(record));
        }
    }

    std::sort(applications.begin(), applications.end(), [](const ApplicationRecord& left,
                                                            const ApplicationRecord& right) {
        const std::string leftName = lowerAscii(left.info.name);
        const std::string rightName = lowerAscii(right.info.name);
        return leftName == rightName ? left.info.id < right.info.id : leftName < rightName;
    });
    std::unordered_set<std::string> seenIds;
    applications.erase(
        std::remove_if(applications.begin(), applications.end(),
                       [&seenIds](const ApplicationRecord& application) {
                           return !seenIds.insert(application.info.id).second;
                       }),
        applications.end());

    if (applications.empty()) {
        return {false, "Windows AppsFolder did not report any launchable applications."};
    }
    return {true, {}};
}

ApplicationListResult ApplicationCatalogPrivate::listApplications()
{
    std::vector<ApplicationRecord> records;
    const OperationResult operation = discover(records);
    ApplicationListResult result;
    result.operation = operation;
    if (!operation.success) {
        return result;
    }
    result.applications.reserve(records.size());
    for (ApplicationRecord& record : records) {
        result.applications.push_back(std::move(record.info));
    }
    return result;
}

OperationResult ApplicationCatalogPrivate::openApplication(const std::string& idOrName)
{
    if (idOrName.empty()) {
        return {false, "Application ID or name cannot be empty."};
    }

    std::vector<ApplicationRecord> applications;
    const OperationResult discovery = discover(applications);
    if (!discovery.success) {
        return discovery;
    }

    const ApplicationRecord* selected = nullptr;
    for (const ApplicationRecord& application : applications) {
        if (application.info.id == idOrName) {
            selected = &application;
            break;
        }
    }
    if (selected == nullptr) {
        const std::string normalizedQuery = lowerAscii(idOrName);
        for (const ApplicationRecord& application : applications) {
            if (lowerAscii(application.info.name) != normalizedQuery) {
                continue;
            }
            if (selected != nullptr) {
                return {false, "Application name is ambiguous; use an application ID."};
            }
            selected = &application;
        }
    }
    if (selected == nullptr) {
        return {false, "Application was not found: " + idOrName};
    }

    DWORD processId = 0;
    HRESULT activationResult = E_NOTIMPL;
    if (!selected->applicationUserModelId.empty()) {
        Microsoft::WRL::ComPtr<IApplicationActivationManager> activationManager;
        activationResult = CoCreateInstance(CLSID_ApplicationActivationManager, nullptr,
                                            CLSCTX_INPROC_SERVER,
                                            IID_PPV_ARGS(&activationManager));
        if (SUCCEEDED(activationResult)) {
            activationResult = activationManager->ActivateApplication(
                selected->applicationUserModelId.c_str(), nullptr, AO_NONE, &processId);
        }
        if (SUCCEEDED(activationResult)) {
            std::string message = "Application opened: " + selected->info.name;
            if (processId != 0) {
                message += " (process id " + std::to_string(processId) + ")";
            }
            return {true, message};
        }
    }

    SHELLEXECUTEINFOW execution{};
    execution.cbSize = sizeof(execution);
    execution.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    execution.lpVerb = L"open";
    std::wstring appsFolderArgument;
    bool usesExplorerBroker = false;
    if (selected->applicationUserModelId.empty()) {
        execution.lpFile = selected->launchTarget.c_str();
    } else {
        // Explorer is the documented compatibility broker for non-packaged AppsFolder items.
        appsFolderArgument = L"\"shell:AppsFolder\\" + selected->applicationUserModelId + L"\"";
        execution.lpFile = L"explorer.exe";
        execution.lpParameters = appsFolderArgument.c_str();
        usesExplorerBroker = true;
    }
    execution.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&execution) == FALSE) {
        std::string message = "Windows Shell rejected the application launch request with error "
            + std::to_string(static_cast<unsigned long>(GetLastError())) + ".";
        if (!selected->applicationUserModelId.empty()) {
            message += " App activation also failed with HRESULT "
                + std::to_string(static_cast<unsigned long>(activationResult)) + ".";
        }
        return {false, message};
    }

    if (execution.hProcess != nullptr) {
        if (!usesExplorerBroker) {
            processId = GetProcessId(execution.hProcess);
        }
        CloseHandle(execution.hProcess);
    }
    std::string message = "Application opened: " + selected->info.name;
    if (processId != 0) {
        message += " (process id " + std::to_string(processId) + ")";
    }
    return {true, message};
}

} // namespace desktopctl
