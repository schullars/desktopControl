#include "private/applicationCatalogPrivate.h"

#include <gio/gdesktopappinfo.h>

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace desktopctl {

ApplicationCatalogPrivate::ApplicationCatalogPrivate() = default;

ApplicationCatalogPrivate::~ApplicationCatalogPrivate() = default;

std::string ApplicationCatalogPrivate::lowerAscii(const std::string& value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

OperationResult ApplicationCatalogPrivate::discover(
    std::vector<ApplicationRecord>& applications)
{
    applications.clear();
    GList* applicationList = g_app_info_get_all();
    for (GList* item = applicationList; item != nullptr; item = item->next) {
        GAppInfo* application = G_APP_INFO(item->data);
        const char* id = g_app_info_get_id(application);
        const char* name = g_app_info_get_display_name(application);
        if (id == nullptr || *id == '\0' || name == nullptr || *name == '\0'
            || g_app_info_should_show(application) == FALSE) {
            continue;
        }
        ApplicationRecord record;
        record.desktopId = id;
        record.info.id = "desktop:" + record.desktopId;
        record.info.name = name;
        applications.push_back(std::move(record));
    }
    g_list_free_full(applicationList, g_object_unref);

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
        return {false, "The Linux desktop application catalog did not report any launchable "
                       "applications."};
    }
    return {true, {}};
}

ApplicationListResult ApplicationCatalogPrivate::listApplications()
{
    std::vector<ApplicationRecord> records;
    ApplicationListResult result;
    result.operation = discover(records);
    if (!result.operation.success) {
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
    OperationResult discovery = discover(applications);
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
        for (const ApplicationRecord& application : applications) {
            if (application.info.name == idOrName) {
                if (selected != nullptr) {
                    return {false, "Application name is ambiguous; use an ID from app list."};
                }
                selected = &application;
            }
        }
    }
    if (selected == nullptr) {
        return {false, "Application was not found: " + idOrName};
    }

    GDesktopAppInfo* desktopInfo = g_desktop_app_info_new(selected->desktopId.c_str());
    if (desktopInfo == nullptr) {
        return {false, "Unable to load the Linux desktop entry: " + selected->desktopId};
    }
    GError* error = nullptr;
    const gboolean launched = g_app_info_launch(G_APP_INFO(desktopInfo), nullptr, nullptr, &error);
    g_object_unref(desktopInfo);
    if (launched == FALSE) {
        const std::string message = error != nullptr && error->message != nullptr
            ? error->message
            : "unknown error";
        g_clear_error(&error);
        return {false, "Unable to launch the Linux application: " + message + "."};
    }
    return {true, "Application launch requested: " + selected->info.name};
}

} // namespace desktopctl
