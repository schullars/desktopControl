#include "platform/linux/private/linuxActivityOverlay.h"

#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include <dlfcn.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace desktopctl {

LinuxActivityOverlay::LinuxActivityOverlay() = default;

LinuxActivityOverlay::~LinuxActivityOverlay()
{
    stop();
}

bool LinuxActivityOverlay::MonitorBounds::operator==(const MonitorBounds& other) const
{
    return x == other.x && y == other.y && width == other.width && height == other.height;
}

OperationResult LinuxActivityOverlay::start()
{
    if (display_ != nullptr) {
        return {true, {}};
    }
    display_ = XOpenDisplay(nullptr);
    if (display_ == nullptr) {
        return {false,
                "Unable to open the X11 display required for the green Linux activity overlay. "
                "Ubuntu Wayland sessions must keep XWayland enabled."};
    }
    root_ = DefaultRootWindow(display_);
    colormap_ = DefaultColormap(display_, DefaultScreen(display_));
    opacityAtom_ = XInternAtom(display_, "_NET_WM_WINDOW_OPACITY", False);

    XColor color{};
    color.red = 40U * 257U;
    color.green = 245U * 257U;
    color.blue = 112U * 257U;
    if (XAllocColor(display_, colormap_, &color) == 0) {
        stop();
        return {false, "Unable to allocate the green X11 activity-indicator color."};
    }
    greenPixel_ = color.pixel;

    randrLibrary_ = dlopen("libXrandr.so.2", RTLD_LAZY | RTLD_LOCAL);
    if (randrLibrary_ != nullptr) {
        xrrGetMonitors_ = reinterpret_cast<XrrGetMonitors>(
            dlsym(randrLibrary_, "XRRGetMonitors"));
        xrrFreeMonitors_ = reinterpret_cast<XrrFreeMonitors>(
            dlsym(randrLibrary_, "XRRFreeMonitors"));
        if (xrrGetMonitors_ == nullptr || xrrFreeMonitors_ == nullptr) {
            dlclose(randrLibrary_);
            randrLibrary_ = nullptr;
            xrrGetMonitors_ = nullptr;
            xrrFreeMonitors_ = nullptr;
        }
    }

    std::vector<MonitorBounds> monitors;
    OperationResult monitorResult = discoverMonitors(monitors);
    if (!monitorResult.success) {
        stop();
        return monitorResult;
    }
    OperationResult borderResult = rebuildBorders(monitors);
    if (!borderResult.success) {
        stop();
        return borderResult;
    }
    OperationResult cursorResult = createCursorOverlay();
    if (!cursorResult.success) {
        stop();
        return cursorResult;
    }
    monitors_ = std::move(monitors);
    lastMonitorCheck_ = std::chrono::steady_clock::now();
    lastRaise_ = lastMonitorCheck_;
    updateCursorPosition();
    raiseWindows();
    XFlush(display_);
    return {true, {}};
}

void LinuxActivityOverlay::stop()
{
    if (display_ != nullptr) {
        destroyWindows();
        XFlush(display_);
        XCloseDisplay(display_);
        display_ = nullptr;
    }
    if (randrLibrary_ != nullptr) {
        dlclose(randrLibrary_);
        randrLibrary_ = nullptr;
    }
    xrrGetMonitors_ = nullptr;
    xrrFreeMonitors_ = nullptr;
    root_ = None;
    colormap_ = None;
    greenPixel_ = 0;
    opacityAtom_ = None;
    monitors_.clear();
}

int LinuxActivityOverlay::descriptor() const
{
    return display_ == nullptr ? -1 : ConnectionNumber(display_);
}

void LinuxActivityOverlay::processEvents()
{
    if (display_ == nullptr) {
        return;
    }
    while (XPending(display_) > 0) {
        XEvent event{};
        XNextEvent(display_, &event);
    }
}

OperationResult LinuxActivityOverlay::update()
{
    if (display_ == nullptr) {
        return {false, "The X11 activity overlay is not running."};
    }
    updateCursorPosition();
    const auto now = std::chrono::steady_clock::now();
    if (now - lastRaise_ >= std::chrono::milliseconds(250)) {
        raiseWindows();
        lastRaise_ = now;
    }
    if (now - lastMonitorCheck_ >= std::chrono::seconds(1)) {
        std::vector<MonitorBounds> monitors;
        OperationResult result = discoverMonitors(monitors);
        if (!result.success) {
            return result;
        }
        if (monitors != monitors_) {
            result = rebuildBorders(monitors);
            if (!result.success) {
                return result;
            }
            monitors_ = std::move(monitors);
        }
        lastMonitorCheck_ = now;
    }
    XFlush(display_);
    return {true, {}};
}

OperationResult LinuxActivityOverlay::discoverMonitors(std::vector<MonitorBounds>& monitors)
{
    monitors.clear();
    if (xrrGetMonitors_ != nullptr && xrrFreeMonitors_ != nullptr) {
        int count = 0;
        XrrMonitorInfo* records = xrrGetMonitors_(display_, root_, True, &count);
        if (records != nullptr) {
            for (int index = 0; index < count; ++index) {
                const XrrMonitorInfo& record = records[index];
                if (record.width > 0 && record.height > 0) {
                    monitors.push_back({record.x, record.y, record.width, record.height});
                }
            }
            xrrFreeMonitors_(records);
        }
    }
    if (monitors.empty()) {
        XWindowAttributes attributes{};
        if (XGetWindowAttributes(display_, root_, &attributes) == 0
            || attributes.width <= 0 || attributes.height <= 0) {
            return {false, "Unable to read the X11 virtual desktop bounds."};
        }
        monitors.push_back({0, 0, attributes.width, attributes.height});
    }
    std::sort(monitors.begin(), monitors.end(), [](const MonitorBounds& left,
                                                   const MonitorBounds& right) {
        return left.x == right.x ? left.y < right.y : left.x < right.x;
    });
    return {true, {}};
}

OperationResult LinuxActivityOverlay::rebuildBorders(
    const std::vector<MonitorBounds>& monitors)
{
    for (Window window : borders_) {
        XDestroyWindow(display_, window);
    }
    borders_.clear();
    for (const MonitorBounds& monitor : monitors) {
        const int thickness = std::max(1, std::min({BorderThickness, monitor.width / 2,
                                                    monitor.height / 2}));
        for (int depth = 0; depth < thickness; ++depth) {
            const std::uint64_t alpha = static_cast<std::uint64_t>(145 * (thickness - depth))
                / static_cast<std::uint64_t>(thickness);
            const unsigned long opacity = static_cast<unsigned long>(
                alpha * 0xffffffffULL / 255ULL);
            const std::array<MonitorBounds, 4> strips{{
                {monitor.x, monitor.y + depth, monitor.width, 1},
                {monitor.x, monitor.y + monitor.height - depth - 1, monitor.width, 1},
                {monitor.x + depth, monitor.y, 1, monitor.height},
                {monitor.x + monitor.width - depth - 1, monitor.y, 1, monitor.height}
            }};
            for (const MonitorBounds& strip : strips) {
                Window window = createOverlayWindow(strip.x, strip.y, strip.width,
                                                    strip.height, opacity);
                if (window == None) {
                    return {false, "Unable to create an X11 activity-indicator border window."};
                }
                borders_.push_back(window);
            }
        }
    }
    XFlush(display_);
    return {true, {}};
}

Window LinuxActivityOverlay::createOverlayWindow(int x, int y, int width, int height,
                                                  unsigned long opacity)
{
    XSetWindowAttributes attributes{};
    attributes.override_redirect = True;
    attributes.background_pixel = greenPixel_;
    attributes.event_mask = StructureNotifyMask;
    Window window = XCreateWindow(display_, root_, x, y,
                                  static_cast<unsigned int>(width),
                                  static_cast<unsigned int>(height), 0, CopyFromParent,
                                  InputOutput, CopyFromParent,
                                  CWOverrideRedirect | CWBackPixel | CWEventMask, &attributes);
    if (window == None) {
        return None;
    }
    XChangeProperty(display_, window, opacityAtom_, XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&opacity), 1);
    makeInputTransparent(window);
    XMapRaised(display_, window);
    return window;
}

OperationResult LinuxActivityOverlay::createCursorOverlay()
{
    const unsigned long opacity = static_cast<unsigned long>(235ULL * 0xffffffffULL / 255ULL);
    cursorWindow_ = createOverlayWindow(0, 0, CursorOverlaySize, CursorOverlaySize, opacity);
    if (cursorWindow_ == None) {
        return {false, "Unable to create the X11 pointer halo window."};
    }

    Pixmap mask = XCreatePixmap(display_, cursorWindow_, CursorOverlaySize,
                                CursorOverlaySize, 1);
    if (mask == None) {
        return {false, "Unable to create the X11 pointer halo shape mask."};
    }
    GC graphics = XCreateGC(display_, mask, 0, nullptr);
    XSetForeground(display_, graphics, 0);
    XFillRectangle(display_, mask, graphics, 0, 0, CursorOverlaySize, CursorOverlaySize);
    XSetForeground(display_, graphics, 1);
    XFillArc(display_, mask, graphics, 10, 10, 44, 44, 0, 360 * 64);
    XSetForeground(display_, graphics, 0);
    XFillArc(display_, mask, graphics, 16, 16, 32, 32, 0, 360 * 64);
    XShapeCombineMask(display_, cursorWindow_, ShapeBounding, 0, 0, mask, ShapeSet);
    XFreeGC(display_, graphics);
    XFreePixmap(display_, mask);
    makeInputTransparent(cursorWindow_);
    return {true, {}};
}

void LinuxActivityOverlay::makeInputTransparent(Window window)
{
    XShapeCombineRectangles(display_, window, ShapeInput, 0, 0, nullptr, 0, ShapeSet,
                            Unsorted);
}

void LinuxActivityOverlay::raiseWindows()
{
    for (Window window : borders_) {
        XRaiseWindow(display_, window);
    }
    if (cursorWindow_ != None) {
        XRaiseWindow(display_, cursorWindow_);
    }
}

void LinuxActivityOverlay::updateCursorPosition()
{
    if (cursorWindow_ == None) {
        return;
    }
    Window rootReturn = None;
    Window childReturn = None;
    int rootX = 0;
    int rootY = 0;
    int windowX = 0;
    int windowY = 0;
    unsigned int mask = 0;
    if (XQueryPointer(display_, root_, &rootReturn, &childReturn, &rootX, &rootY,
                      &windowX, &windowY, &mask) != False) {
        XMoveWindow(display_, cursorWindow_, rootX - CursorOverlaySize / 2,
                    rootY - CursorOverlaySize / 2);
    }
}

void LinuxActivityOverlay::destroyWindows()
{
    for (Window window : borders_) {
        XDestroyWindow(display_, window);
    }
    borders_.clear();
    if (cursorWindow_ != None) {
        XDestroyWindow(display_, cursorWindow_);
        cursorWindow_ = None;
    }
}

} // namespace desktopctl
