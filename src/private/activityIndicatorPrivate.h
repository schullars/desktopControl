#pragma once

#include "desktopAutomation.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>

#include <atomic>
#elif defined(__linux__)
#include <csignal>
#endif

namespace desktopctl {

class ActivityIndicatorPrivate final {
public:
    ActivityIndicatorPrivate();
    ~ActivityIndicatorPrivate();

    OperationResult startBackground();
    bool isRunning() const;
    OperationResult recordActivity();
    OperationResult run();
    OperationResult requestStop();

private:
#if defined(_WIN32)
    enum class BorderSide {
        Top,
        Bottom,
        Left,
        Right
    };

    struct BorderWindow {
        HWND handle = nullptr;
        BorderSide side = BorderSide::Top;
    };

    static constexpr UINT_PTR CursorTimerId = 1;
    static constexpr UINT CursorTimerIntervalMilliseconds = 16;
    static constexpr ULONGLONG BorderZOrderMaintenanceIntervalMilliseconds = 250;
    static constexpr int BorderThickness = 14;
    static constexpr int BorderMaximumAlpha = 145;
    static constexpr int CursorOverlaySize = 64;
    static constexpr DWORD SessionIdleTimeoutMilliseconds = 3U * 60U * 1000U;
    inline static constexpr wchar_t SessionMutexName[] =
        L"Local\\desktopctl-green-activity-indicator";
    inline static constexpr wchar_t StopEventName[] =
        L"Local\\desktopctl-green-activity-stop";
    inline static constexpr wchar_t ReadyEventName[] =
        L"Local\\desktopctl-green-activity-ready";
    inline static constexpr wchar_t ActivityEventName[] =
        L"Local\\desktopctl-green-activity-command";

    static std::atomic<DWORD> sessionThreadId_;
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam,
                                            LPARAM lParam);
    static BOOL WINAPI consoleControlHandler(DWORD controlType);
    static BOOL CALLBACK monitorEnumerationProcedure(HMONITOR monitor, HDC deviceContext,
                                                     LPRECT monitorRectangle, LPARAM context);
    LRESULT handleWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    bool registerWindowClass();
    HWND createOverlayWindow();
    bool createOverlays();
    void destroyOverlays();
    bool configureBorders();
    bool enumerateMonitorBounds(std::vector<RECT>& monitorBounds) const;
    bool createMonitorBorders(const RECT& monitorBounds,
                              std::vector<BorderWindow>& borderWindows);
    static void destroyBorderWindows(std::vector<BorderWindow>& borderWindows);
    bool renderBorder(const BorderWindow& border, int x, int y, int width, int height);
    bool renderCursor();
    void updateCursorPosition();
    void maintainOverlayZOrder();
    bool updateLayeredPixels(HWND window, int x, int y, int width, int height,
                             const std::vector<std::uint32_t>& pixels);

    std::vector<BorderWindow> borders_;
    HWND cursorWindow_ = nullptr;
    ULONGLONG lastBorderZOrderMaintenanceTime_ = 0;
    HANDLE sessionMutex_ = nullptr;
    HANDLE stopEvent_ = nullptr;
    HANDLE readyEvent_ = nullptr;
    HANDLE activityEvent_ = nullptr;
#elif defined(__linux__)
    static constexpr int SessionIdleTimeoutSeconds = 3 * 60;

    static void handleSignal(int signalNumber);
    OperationResult handleLinuxRequest(const std::string& request, bool& stopRequested);

    inline static volatile std::sig_atomic_t stopSignal_ = 0;
    std::unique_ptr<class LinuxSessionServer> linuxServer_;
    std::unique_ptr<class LinuxActivityOverlay> linuxOverlay_;
    std::unique_ptr<class LinuxPortalRemoteDesktop> linuxPortal_;
#endif
};

} // namespace desktopctl
