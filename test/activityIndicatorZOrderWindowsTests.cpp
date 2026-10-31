#include <Windows.h>

#include <chrono>
#include <cwchar>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

struct OverlayWindows {
    std::vector<HWND> borders;
};

BOOL CALLBACK collectOverlayWindows(HWND window, LPARAM context)
{
    wchar_t className[128]{};
    if (GetClassNameW(window, className, static_cast<int>(std::size(className))) == 0
        || std::wcscmp(className, L"desktopctl.GreenActivityOverlay") != 0
        || IsWindowVisible(window) == FALSE) {
        return TRUE;
    }

    RECT bounds{};
    if (GetWindowRect(window, &bounds) == FALSE) {
        return TRUE;
    }
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width != height && (width > 64 || height > 64)) {
        auto* overlays = reinterpret_cast<OverlayWindows*>(context);
        overlays->borders.push_back(window);
    }
    return TRUE;
}

int runDesktopCtl(const std::wstring& executable, const std::wstring& arguments)
{
    std::wstring commandLine = L'"' + executable + L"\" " + arguments;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr,
                       &startup, &process)
        == FALSE) {
        return -1;
    }

    const DWORD waitResult = WaitForSingleObject(process.hProcess, 5000);
    DWORD exitCode = static_cast<DWORD>(-1);
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(process.hProcess, &exitCode);
    } else {
        TerminateProcess(process.hProcess, static_cast<UINT>(-1));
        WaitForSingleObject(process.hProcess, 1000);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}

int zOrderIndex(HWND target)
{
    int index = 0;
    for (HWND window = GetTopWindow(nullptr); window != nullptr;
         window = GetWindow(window, GW_HWNDNEXT), ++index) {
        if (window == target) {
            return index;
        }
    }
    return -1;
}

int wmain(int argumentCount, wchar_t* arguments[])
{
    if (argumentCount != 2) {
        std::cerr << "Expected the desktopctl executable path.\n";
        return 1;
    }

    const std::wstring executable = arguments[1];

    runDesktopCtl(executable, L"session stop");
    if (runDesktopCtl(executable, L"session start") != 0) {
        std::cerr << "Could not start the activity indicator.\n";
        return 1;
    }

    int result = 0;
    HWND occluder = nullptr;
    const wchar_t occluderClass[] = L"desktopctl.ZOrderTestOccluder";
    do {
        OverlayWindows overlays;
        for (int attempt = 0; attempt < 20 && overlays.borders.empty(); ++attempt) {
            overlays.borders.clear();
            EnumWindows(&collectOverlayWindows, reinterpret_cast<LPARAM>(&overlays));
            if (overlays.borders.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        if (overlays.borders.empty()) {
            std::cerr << "No activity-indicator border windows were found.\n";
            result = 1;
            break;
        }

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = occluderClass;
        if (RegisterClassExW(&windowClass) == 0) {
            std::cerr << "Could not register the test occluder window class.\n";
            result = 1;
            break;
        }
        occluder = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                   occluderClass, L"", WS_POPUP, -32000, -32000, 2, 2,
                                   nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (occluder == nullptr
            || SetWindowPos(occluder, HWND_TOPMOST, -32000, -32000, 2, 2,
                            SWP_NOACTIVATE | SWP_SHOWWINDOW)
                == FALSE) {
            std::cerr << "Could not create the topmost test occluder.\n";
            result = 1;
            break;
        }

        const int initialOccluderIndex = zOrderIndex(occluder);
        bool displacedBorder = false;
        for (const HWND border : overlays.borders) {
            const int borderIndex = zOrderIndex(border);
            displacedBorder = displacedBorder
                || (borderIndex >= 0 && initialOccluderIndex >= 0
                    && borderIndex > initialOccluderIndex);
        }
        if (!displacedBorder) {
            std::cerr << "The test occluder did not move above a border window.\n";
            result = 1;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        const int finalOccluderIndex = zOrderIndex(occluder);
        for (const HWND border : overlays.borders) {
            const int borderIndex = zOrderIndex(border);
            if (borderIndex < 0 || finalOccluderIndex < 0 || borderIndex > finalOccluderIndex) {
                std::cerr << "A border window remained below the topmost occluder.\n";
                result = 1;
                break;
            }
        }
    } while (false);

    if (occluder != nullptr) {
        DestroyWindow(occluder);
    }
    UnregisterClassW(occluderClass, GetModuleHandleW(nullptr));
    if (runDesktopCtl(executable, L"session stop") != 0) {
        std::cerr << "Could not stop the activity indicator after the test.\n";
        result = 1;
    }
    if (result == 0) {
        std::cout << "Activity-indicator border z-order recovered.\n";
    }
    return result;
}
