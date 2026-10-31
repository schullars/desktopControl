#pragma once

#include "operationResult.h"

#include <X11/Xlib.h>

#include <chrono>
#include <vector>

namespace desktopctl {

class LinuxActivityOverlay final {
public:
    LinuxActivityOverlay();
    ~LinuxActivityOverlay();

    LinuxActivityOverlay(const LinuxActivityOverlay&) = delete;
    LinuxActivityOverlay& operator=(const LinuxActivityOverlay&) = delete;

    OperationResult start();
    void stop();
    int descriptor() const;
    void processEvents();
    OperationResult update();

private:
    struct MonitorBounds {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        bool operator==(const MonitorBounds& other) const;
    };

    struct XrrMonitorInfo {
        Atom name;
        Bool primary;
        Bool automatic;
        int outputCount;
        int x;
        int y;
        int width;
        int height;
        int physicalWidth;
        int physicalHeight;
        XID* outputs;
    };

    using XrrGetMonitors = XrrMonitorInfo* (*)(Display*, Window, Bool, int*);
    using XrrFreeMonitors = void (*)(XrrMonitorInfo*);

    OperationResult discoverMonitors(std::vector<MonitorBounds>& monitors);
    OperationResult rebuildBorders(const std::vector<MonitorBounds>& monitors);
    Window createOverlayWindow(int x, int y, int width, int height,
                               unsigned long opacity);
    OperationResult createCursorOverlay();
    void makeInputTransparent(Window window);
    void raiseWindows();
    void updateCursorPosition();
    void destroyWindows();

    static constexpr int BorderThickness = 14;
    static constexpr int CursorOverlaySize = 64;

    Display* display_ = nullptr;
    Window root_ = None;
    Colormap colormap_ = None;
    unsigned long greenPixel_ = 0;
    Atom opacityAtom_ = None;
    void* randrLibrary_ = nullptr;
    XrrGetMonitors xrrGetMonitors_ = nullptr;
    XrrFreeMonitors xrrFreeMonitors_ = nullptr;
    std::vector<MonitorBounds> monitors_;
    std::vector<Window> borders_;
    Window cursorWindow_ = None;
    std::chrono::steady_clock::time_point lastMonitorCheck_{};
    std::chrono::steady_clock::time_point lastRaise_{};
};

} // namespace desktopctl
