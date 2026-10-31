#include "private/activityIndicatorPrivate.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace desktopctl {

std::atomic<DWORD> ActivityIndicatorPrivate::sessionThreadId_{0};

ActivityIndicatorPrivate::ActivityIndicatorPrivate() = default;

ActivityIndicatorPrivate::~ActivityIndicatorPrivate()
{
    destroyOverlays();
    if (activityEvent_ != nullptr) {
        CloseHandle(activityEvent_);
        activityEvent_ = nullptr;
    }
    if (keepAliveEvent_ != nullptr) {
        CloseHandle(keepAliveEvent_);
        keepAliveEvent_ = nullptr;
    }
    if (readyEvent_ != nullptr) {
        CloseHandle(readyEvent_);
        readyEvent_ = nullptr;
    }
    if (stopEvent_ != nullptr) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
    if (sessionMutex_ != nullptr) {
        ReleaseMutex(sessionMutex_);
        CloseHandle(sessionMutex_);
        sessionMutex_ = nullptr;
    }
}

bool ActivityIndicatorPrivate::isRunning() const
{
    HANDLE readyEvent = OpenEventW(SYNCHRONIZE, FALSE, ReadyEventName);
    if (readyEvent == nullptr) {
        return false;
    }
    const bool running = WaitForSingleObject(readyEvent, 0) == WAIT_OBJECT_0;
    CloseHandle(readyEvent);
    return running;
}

OperationResult ActivityIndicatorPrivate::startBackground(bool keepAlive)
{
    if (isRunning()) {
        if (keepAlive && !isKeepAliveSession()) {
            return {false, "A timed session is already running. Stop it before starting "
                           "keep-alive mode."};
        }
        const OperationResult activityResult = recordActivity();
        if (!activityResult.success) {
            return activityResult;
        }
        return {true, "Session is already started. Green activity indicator is running."};
    }

    std::wstring executablePath(32768, L'\0');
    const DWORD pathLength = GetModuleFileNameW(nullptr, executablePath.data(),
                                                static_cast<DWORD>(executablePath.size()));
    if (pathLength == 0 || pathLength >= executablePath.size()) {
        return {false, "Unable to resolve the desktopctl executable path."};
    }
    executablePath.resize(pathLength);
    std::wstring commandLine = L'"' + executablePath + L"\" session host";
    if (keepAlive) {
        commandLine += L" --keep-alive";
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(executablePath.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr,
                       &startup, &process)
        == FALSE) {
        return {false,
                "Unable to start the background session process. Win32 error "
                    + std::to_string(static_cast<unsigned long>(GetLastError())) + "."};
    }

    bool ready = false;
    DWORD exitCode = STILL_ACTIVE;
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (isRunning()) {
            ready = true;
            break;
        }
        if (WaitForSingleObject(process.hProcess, 20) == WAIT_OBJECT_0) {
            GetExitCodeProcess(process.hProcess, &exitCode);
            break;
        }
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    if (!ready) {
        return {false,
                exitCode == STILL_ACTIVE
                    ? "The background session did not become ready within two seconds."
                    : "The background session exited before becoming ready with code "
                        + std::to_string(static_cast<unsigned long>(exitCode)) + "."};
    }
    return {true, "Session started. Green activity indicator is running."};
}

OperationResult ActivityIndicatorPrivate::recordActivity()
{
    HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, ActivityEventName);
    if (event == nullptr) {
        return {false, "The session activity channel is unavailable."};
    }
    const BOOL signaled = SetEvent(event);
    CloseHandle(event);
    if (signaled == FALSE) {
        return {false, "Unable to refresh the session inactivity deadline."};
    }
    return {true, {}};
}

OperationResult ActivityIndicatorPrivate::run(bool keepAlive)
{
    sessionMutex_ = CreateMutexW(nullptr, TRUE, SessionMutexName);
    if (sessionMutex_ == nullptr) {
        return {false, "Unable to create the activity-indicator session mutex."};
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(sessionMutex_);
        sessionMutex_ = nullptr;
        return {false, "A desktopctl activity indicator is already running."};
    }

    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, StopEventName);
    if (stopEvent_ == nullptr) {
        return {false, "Unable to create the activity-indicator stop event."};
    }
    readyEvent_ = CreateEventW(nullptr, TRUE, FALSE, ReadyEventName);
    if (readyEvent_ == nullptr) {
        return {false, "Unable to create the activity-indicator ready event."};
    }
    activityEvent_ = CreateEventW(nullptr, FALSE, FALSE, ActivityEventName);
    if (activityEvent_ == nullptr) {
        return {false, "Unable to create the activity-indicator activity event."};
    }
    keepAliveEvent_ = CreateEventW(nullptr, TRUE, keepAlive ? TRUE : FALSE,
                                   KeepAliveEventName);
    if (keepAliveEvent_ == nullptr) {
        return {false, "Unable to create the keep-alive session state."};
    }
    if (!createOverlays()) {
        return {false, "Unable to create one or more activity-indicator overlay windows."};
    }
    lastBorderZOrderMaintenanceTime_ = GetTickCount64();
    if (SetTimer(cursorWindow_, CursorTimerId, CursorTimerIntervalMilliseconds, nullptr) == 0) {
        return {false, "Unable to start the activity-indicator cursor timer."};
    }
    if (SetEvent(readyEvent_) == FALSE) {
        return {false, "Unable to publish the activity-indicator ready state."};
    }

    sessionThreadId_.store(GetCurrentThreadId());
    SetConsoleCtrlHandler(&ActivityIndicatorPrivate::consoleControlHandler, TRUE);

    bool running = true;
    bool messageFailure = false;
    bool idleTimeout = false;
    ULONGLONG lastActivityTime = GetTickCount64();
    HANDLE waitHandles[] = {stopEvent_, activityEvent_};
    while (running) {
        const ULONGLONG currentTime = GetTickCount64();
        const ULONGLONG idleDuration = currentTime - lastActivityTime;
        if (!keepAlive && idleDuration >= SessionIdleTimeoutMilliseconds) {
            idleTimeout = true;
            break;
        }
        const DWORD remainingTime = keepAlive
            ? INFINITE
            : static_cast<DWORD>(SessionIdleTimeoutMilliseconds - idleDuration);
        const DWORD waitResult = MsgWaitForMultipleObjects(2, waitHandles, FALSE, remainingTime,
                                                           QS_ALLINPUT);
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }
        if (waitResult == WAIT_OBJECT_0 + 1U) {
            lastActivityTime = GetTickCount64();
            continue;
        }
        if (waitResult == WAIT_TIMEOUT) {
            idleTimeout = true;
            break;
        }
        if (waitResult != WAIT_OBJECT_0 + 2U) {
            messageFailure = true;
            break;
        }

        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    SetConsoleCtrlHandler(&ActivityIndicatorPrivate::consoleControlHandler, FALSE);
    sessionThreadId_.store(0);
    ResetEvent(readyEvent_);
    KillTimer(cursorWindow_, CursorTimerId);
    destroyOverlays();

    if (messageFailure) {
        return {false, "The activity-indicator message loop failed."};
    }
    if (idleTimeout) {
        return {true, "Session stopped after three minutes of inactivity."};
    }
    return {true, "Green activity indicator stopped."};
}

bool ActivityIndicatorPrivate::isKeepAliveSession() const
{
    HANDLE event = OpenEventW(SYNCHRONIZE, FALSE, KeepAliveEventName);
    if (event == nullptr) {
        return false;
    }
    const bool keepAlive = WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
    CloseHandle(event);
    return keepAlive;
}

OperationResult ActivityIndicatorPrivate::requestStop()
{
    HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, StopEventName);
    if (event == nullptr) {
        return {false, "No desktopctl activity indicator is running."};
    }
    const BOOL signaled = SetEvent(event);
    CloseHandle(event);
    if (signaled == FALSE) {
        return {false, "Unable to stop the desktopctl activity indicator."};
    }
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (!isRunning()) {
            return {true, "Session stopped. Green activity indicator closed."};
        }
        Sleep(20);
    }
    return {false, "The activity indicator did not stop within two seconds."};
}

BOOL WINAPI ActivityIndicatorPrivate::consoleControlHandler(DWORD controlType)
{
    if (controlType != CTRL_C_EVENT && controlType != CTRL_BREAK_EVENT
        && controlType != CTRL_CLOSE_EVENT) {
        return FALSE;
    }

    const DWORD threadId = sessionThreadId_.load();
    if (threadId != 0) {
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
        return TRUE;
    }
    return FALSE;
}

LRESULT CALLBACK ActivityIndicatorPrivate::windowProcedure(HWND window, UINT message,
                                                            WPARAM wParam, LPARAM lParam)
{
    ActivityIndicatorPrivate* indicator = reinterpret_cast<ActivityIndicatorPrivate*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const CREATESTRUCTW* creation = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        indicator = static_cast<ActivityIndicatorPrivate*>(creation->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(indicator));
    }
    if (indicator != nullptr) {
        return indicator->handleWindowMessage(window, message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT ActivityIndicatorPrivate::handleWindowMessage(HWND window, UINT message,
                                                       WPARAM wParam, LPARAM lParam)
{
    if (message == WM_TIMER && window == cursorWindow_ && wParam == CursorTimerId) {
        updateCursorPosition();
        const ULONGLONG currentTime = GetTickCount64();
        if (currentTime - lastBorderZOrderMaintenanceTime_
            >= BorderZOrderMaintenanceIntervalMilliseconds) {
            maintainOverlayZOrder();
            lastBorderZOrderMaintenanceTime_ = currentTime;
        }
        return 0;
    }
    if (message == WM_DISPLAYCHANGE && window == cursorWindow_) {
        configureBorders();
        return 0;
    }
    if (message == WM_NCHITTEST) {
        return HTTRANSPARENT;
    }
    if (message == WM_MOUSEACTIVATE) {
        return MA_NOACTIVATE;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool ActivityIndicatorPrivate::registerWindowClass()
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &ActivityIndicatorPrivate::windowProcedure;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"desktopctl.GreenActivityOverlay";
    if (RegisterClassExW(&windowClass) != 0) {
        return true;
    }
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

HWND ActivityIndicatorPrivate::createOverlayWindow()
{
    return CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW
                               | WS_EX_NOACTIVATE,
                           L"desktopctl.GreenActivityOverlay", L"", WS_POPUP, 0, 0, 1, 1,
                           nullptr, nullptr, GetModuleHandleW(nullptr), this);
}

bool ActivityIndicatorPrivate::createOverlays()
{
    if (!registerWindowClass()) {
        return false;
    }

    cursorWindow_ = createOverlayWindow();
    if (cursorWindow_ == nullptr || !configureBorders() || !renderCursor()) {
        return false;
    }
    updateCursorPosition();
    return true;
}

void ActivityIndicatorPrivate::destroyOverlays()
{
    if (cursorWindow_ != nullptr) {
        DestroyWindow(cursorWindow_);
        cursorWindow_ = nullptr;
    }
    destroyBorderWindows(borders_);
}

BOOL CALLBACK ActivityIndicatorPrivate::monitorEnumerationProcedure(
    HMONITOR, HDC, LPRECT monitorRectangle, LPARAM context)
{
    if (monitorRectangle == nullptr || context == 0) {
        return FALSE;
    }
    auto* monitorBounds = reinterpret_cast<std::vector<RECT>*>(context);
    monitorBounds->push_back(*monitorRectangle);
    return TRUE;
}

bool ActivityIndicatorPrivate::enumerateMonitorBounds(std::vector<RECT>& monitorBounds) const
{
    monitorBounds.clear();
    return EnumDisplayMonitors(nullptr, nullptr,
                               &ActivityIndicatorPrivate::monitorEnumerationProcedure,
                               reinterpret_cast<LPARAM>(&monitorBounds))
            != FALSE
        && !monitorBounds.empty();
}

void ActivityIndicatorPrivate::destroyBorderWindows(
    std::vector<BorderWindow>& borderWindows)
{
    for (BorderWindow& border : borderWindows) {
        if (border.handle != nullptr) {
            DestroyWindow(border.handle);
            border.handle = nullptr;
        }
    }
    borderWindows.clear();
}

bool ActivityIndicatorPrivate::createMonitorBorders(
    const RECT& monitorBounds, std::vector<BorderWindow>& borderWindows)
{
    const int monitorWidth = monitorBounds.right - monitorBounds.left;
    const int monitorHeight = monitorBounds.bottom - monitorBounds.top;
    if (monitorWidth <= 0 || monitorHeight <= 0) {
        return false;
    }
    const int thickness = std::max(1, std::min({BorderThickness, monitorWidth / 2,
                                                monitorHeight / 2}));
    const BorderSide sides[] = {BorderSide::Top, BorderSide::Bottom, BorderSide::Left,
                                BorderSide::Right};

    for (const BorderSide side : sides) {
        BorderWindow border{createOverlayWindow(), side};
        if (border.handle == nullptr) {
            return false;
        }
        int x = monitorBounds.left;
        int y = monitorBounds.top;
        int width = monitorWidth;
        int height = thickness;
        if (side == BorderSide::Bottom) {
            y = monitorBounds.bottom - thickness;
        } else if (side == BorderSide::Left) {
            width = thickness;
            height = monitorHeight;
        } else if (side == BorderSide::Right) {
            x = monitorBounds.right - thickness;
            width = thickness;
            height = monitorHeight;
        }
        if (!renderBorder(border, x, y, width, height)) {
            DestroyWindow(border.handle);
            return false;
        }
        borderWindows.push_back(border);
    }
    return true;
}

bool ActivityIndicatorPrivate::configureBorders()
{
    std::vector<RECT> monitorBounds;
    if (!enumerateMonitorBounds(monitorBounds)) {
        return false;
    }

    std::vector<BorderWindow> replacementBorders;
    replacementBorders.reserve(monitorBounds.size() * 4U);
    for (const RECT& bounds : monitorBounds) {
        if (!createMonitorBorders(bounds, replacementBorders)) {
            destroyBorderWindows(replacementBorders);
            return false;
        }
    }

    destroyBorderWindows(borders_);
    borders_ = std::move(replacementBorders);
    return true;
}

bool ActivityIndicatorPrivate::renderBorder(const BorderWindow& border, int x, int y,
                                             int width, int height)
{
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width)
                                      * static_cast<std::size_t>(height));
    const int depth = border.side == BorderSide::Top || border.side == BorderSide::Bottom
        ? height
        : width;
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            int edgeDistance = row;
            if (border.side == BorderSide::Bottom) {
                edgeDistance = height - row - 1;
            } else if (border.side == BorderSide::Left) {
                edgeDistance = column;
            } else if (border.side == BorderSide::Right) {
                edgeDistance = width - column - 1;
            }
            const int alpha = BorderMaximumAlpha * (depth - edgeDistance) / depth;
            const std::uint32_t red = static_cast<std::uint32_t>(40 * alpha / 255);
            const std::uint32_t green = static_cast<std::uint32_t>(245 * alpha / 255);
            const std::uint32_t blue = static_cast<std::uint32_t>(112 * alpha / 255);
            pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(width)
                   + static_cast<std::size_t>(column)] =
                (static_cast<std::uint32_t>(alpha) << 24U) | (red << 16U) | (green << 8U)
                | blue;
        }
    }
    if (!updateLayeredPixels(border.handle, x, y, width, height, pixels)) {
        return false;
    }
    return SetWindowPos(border.handle, HWND_TOPMOST, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW)
        != FALSE;
}

bool ActivityIndicatorPrivate::renderCursor()
{
    std::vector<std::uint32_t> pixels(CursorOverlaySize * CursorOverlaySize, 0U);
    const double center = (CursorOverlaySize - 1) / 2.0;
    for (int row = 0; row < CursorOverlaySize; ++row) {
        for (int column = 0; column < CursorOverlaySize; ++column) {
            const double deltaX = column - center;
            const double deltaY = row - center;
            const double distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
            int alpha = 0;
            if (distance >= 18.0 && distance <= 22.0) {
                alpha = 235;
            } else if (distance > 22.0 && distance <= 30.0) {
                alpha = static_cast<int>(65.0 * (30.0 - distance) / 8.0);
            } else if (distance >= 15.0 && distance < 18.0) {
                alpha = static_cast<int>(90.0 * (distance - 15.0) / 3.0);
            }
            if (alpha == 0) {
                continue;
            }
            const std::uint32_t red = static_cast<std::uint32_t>(40 * alpha / 255);
            const std::uint32_t green = static_cast<std::uint32_t>(255 * alpha / 255);
            const std::uint32_t blue = static_cast<std::uint32_t>(112 * alpha / 255);
            pixels[static_cast<std::size_t>(row) * CursorOverlaySize
                   + static_cast<std::size_t>(column)] =
                (static_cast<std::uint32_t>(alpha) << 24U) | (red << 16U) | (green << 8U)
                | blue;
        }
    }
    return updateLayeredPixels(cursorWindow_, 0, 0, CursorOverlaySize, CursorOverlaySize,
                               pixels);
}

void ActivityIndicatorPrivate::updateCursorPosition()
{
    POINT cursor{};
    if (GetCursorPos(&cursor) == FALSE || cursorWindow_ == nullptr) {
        return;
    }
    SetWindowPos(cursorWindow_, HWND_TOPMOST, cursor.x - CursorOverlaySize / 2,
                 cursor.y - CursorOverlaySize / 2, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void ActivityIndicatorPrivate::maintainOverlayZOrder()
{
    constexpr UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW;
    // The taskbar is also topmost and can move above long-lived overlays. Reassert only the
    // lightweight Z-order at a low frequency; do not redraw or rebuild monitor windows here.
    for (const BorderWindow& border : borders_) {
        if (border.handle != nullptr) {
            SetWindowPos(border.handle, HWND_TOPMOST, 0, 0, 0, 0, flags);
        }
    }
    if (cursorWindow_ != nullptr) {
        SetWindowPos(cursorWindow_, HWND_TOPMOST, 0, 0, 0, 0, flags);
    }
}

bool ActivityIndicatorPrivate::updateLayeredPixels(
    HWND window, int x, int y, int width, int height,
    const std::vector<std::uint32_t>& pixels)
{
    HDC screenDc = GetDC(nullptr);
    if (screenDc == nullptr) {
        return false;
    }
    HDC memoryDc = CreateCompatibleDC(screenDc);
    if (memoryDc == nullptr) {
        ReleaseDC(nullptr, screenDc);
        return false;
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    void* bitmapBits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &bitmapBits,
                                      nullptr, 0);
    if (bitmap == nullptr || bitmapBits == nullptr) {
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return false;
    }
    std::memcpy(bitmapBits, pixels.data(), pixels.size() * sizeof(std::uint32_t));
    HGDIOBJ previousObject = SelectObject(memoryDc, bitmap);
    if (previousObject == nullptr || previousObject == HGDI_ERROR) {
        DeleteObject(bitmap);
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return false;
    }

    POINT destination{x, y};
    POINT source{0, 0};
    SIZE size{width, height};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    const BOOL updated = UpdateLayeredWindow(window, screenDc, &destination, &size, memoryDc,
                                              &source, 0, &blend, ULW_ALPHA);
    SelectObject(memoryDc, previousObject);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
    return updated != FALSE;
}

} // namespace desktopctl
