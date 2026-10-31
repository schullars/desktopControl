#pragma once

#include "operationResult.h"

#include <X11/Xlib.h>

#include <chrono>
#include <cstdint>
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

    struct XRenderDirectFormatData {
        short red;
        short redMask;
        short green;
        short greenMask;
        short blue;
        short blueMask;
        short alpha;
        short alphaMask;
    };

    struct XRenderPictFormatData {
        XID id;
        int type;
        int depth;
        XRenderDirectFormatData direct;
        Colormap colormap;
    };

    using XrrGetMonitors = XrrMonitorInfo* (*)(Display*, Window, Bool, int*);
    using XrrFreeMonitors = void (*)(XrrMonitorInfo*);
    using XRenderFindVisualFormat = XRenderPictFormatData* (*)(Display*, const Visual*);

    void discoverOverlayVisual();
    bool validateArgbVisual();
    OperationResult discoverMonitors(std::vector<MonitorBounds>& monitors);
    OperationResult rebuildBorders(const std::vector<MonitorBounds>& monitors);
    Window createOverlayWindow(int x, int y, int width, int height,
                               std::uint8_t alpha);
    unsigned long argbPixel(std::uint8_t alpha) const;
    OperationResult createCursorOverlay();
    void makeInputTransparent(Window window);
    void raiseWindows();
    void updateCursorPosition();
    void destroyWindows();
    static int handleOverlayXError(Display* display, XErrorEvent* event);

    static constexpr int BorderThickness = 14;
    static constexpr int WaylandFallbackBorderThickness = 3;
    static constexpr int CursorOverlaySize = 64;

    Display* display_ = nullptr;
    Window root_ = None;
    Colormap colormap_ = None;
    Colormap overlayColormap_ = None;
    Visual* overlayVisual_ = nullptr;
    int overlayDepth_ = 0;
    XRenderDirectFormatData overlayFormat_{};
    bool supportsArgb_ = false;
    unsigned long greenPixel_ = 0;
    Atom opacityAtom_ = None;
    void* randrLibrary_ = nullptr;
    void* xrenderLibrary_ = nullptr;
    XrrGetMonitors xrrGetMonitors_ = nullptr;
    XrrFreeMonitors xrrFreeMonitors_ = nullptr;
    std::vector<MonitorBounds> monitors_;
    std::vector<Window> borders_;
    Window cursorWindow_ = None;
    std::chrono::steady_clock::time_point lastMonitorCheck_{};
    std::chrono::steady_clock::time_point lastRaise_{};
    inline static bool overlayXErrorOccurred_ = false;
};

} // namespace desktopctl
