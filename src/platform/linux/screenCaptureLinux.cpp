#include "private/screenCapturePrivate.h"

#include "platform/linux/private/linuxImageCodec.h"
#include "platform/linux/private/linuxSessionIpc.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <thread>

namespace desktopctl {

ScreenCapturePrivate::ScreenCapturePrivate() = default;

ScreenCapturePrivate::~ScreenCapturePrivate()
{
    if (display_ != nullptr) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
}

WindowListResult ScreenCapturePrivate::listWindows()
{
    if (isWaylandSession()) {
        return {{false,
                 "Standard Wayland portals do not expose a global window list. Use Ubuntu on "
                 "Xorg for window discovery, or address the target application directly."}, {}};
    }
    std::vector<WindowRecord> records;
    WindowListResult result;
    result.operation = discoverWindows(records);
    if (!result.operation.success) {
        return result;
    }
    result.windows.reserve(records.size());
    for (WindowRecord& record : records) {
        result.windows.push_back(std::move(record.info));
    }
    return result;
}

OperationResult ScreenCapturePrivate::activateWindow(const std::string& idOrName)
{
    if (isWaylandSession()) {
        return {false,
                "Wayland intentionally prevents arbitrary applications from forcing another "
                "application to the foreground. Use Ubuntu on Xorg for window activation."};
    }
    std::vector<WindowRecord> windows;
    const WindowRecord* selected = nullptr;
    OperationResult result = resolveWindow(idOrName, windows, selected);
    if (!result.success) {
        return result;
    }

    XMapRaised(display_, selected->handle);
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = selected->handle;
    event.xclient.message_type = activeWindowAtom_;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1;
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(display_, root_, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display_);

    for (int attempt = 0; attempt < 50; ++attempt) {
        unsigned long active = 0;
        if (readCardinal(root_, activeWindowAtom_, active)
            && active == static_cast<unsigned long>(selected->handle)) {
            return {true, "Window activated: " + selected->info.title + " ("
                               + selected->info.id + ")"};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return {false, "The X11 window manager did not grant foreground activation."};
}

ScreenshotResult ScreenCapturePrivate::captureDesktop(
    const std::optional<std::string>& outputPath)
{
    std::filesystem::path resolvedPath;
    OperationResult pathResult = resolveOutputPath(outputPath, "screenshot", resolvedPath);
    if (!pathResult.success) {
        return failureResult(pathResult);
    }
    if (isWaylandSession()) {
        return captureWaylandDesktop(resolvedPath);
    }
    OperationResult displayResult = ensureX11();
    if (!displayResult.success) {
        return failureResult(displayResult);
    }
    XWindowAttributes attributes{};
    if (XGetWindowAttributes(display_, root_, &attributes) == 0) {
        return failureResult({false, "Unable to read the X11 virtual desktop bounds."});
    }
    return captureX11Drawable(root_, 0, 0, attributes.width, attributes.height, resolvedPath);
}

ScreenshotResult ScreenCapturePrivate::captureRegion(
    const ScreenRegion& region, const std::optional<std::string>& outputPath)
{
    if (region.width <= 0 || region.height <= 0) {
        return failureResult({false, "Screenshot width and height must be positive."});
    }
    if (isWaylandSession()) {
        return failureResult(
            {false,
             "Ubuntu 22.04 Wayland portals do not provide deterministic coordinate-based "
             "region capture. Use full capture or log in to Ubuntu on Xorg."});
    }
    OperationResult displayResult = ensureX11();
    if (!displayResult.success) {
        return failureResult(displayResult);
    }
    XWindowAttributes attributes{};
    if (XGetWindowAttributes(display_, root_, &attributes) == 0) {
        return failureResult({false, "Unable to read the X11 virtual desktop bounds."});
    }
    const long long right = static_cast<long long>(region.x) + region.width;
    const long long bottom = static_cast<long long>(region.y) + region.height;
    if (region.x < 0 || region.y < 0 || right > attributes.width
        || bottom > attributes.height) {
        return failureResult(
            {false, "The screenshot region must be fully inside the X11 virtual desktop."});
    }
    std::filesystem::path resolvedPath;
    OperationResult pathResult = resolveOutputPath(outputPath, "screenshot", resolvedPath);
    if (!pathResult.success) {
        return failureResult(pathResult);
    }
    return captureX11Drawable(root_, region.x, region.y, region.width, region.height,
                              resolvedPath);
}

ScreenshotResult ScreenCapturePrivate::captureWindow(
    const std::string& idOrName, const std::optional<std::string>& outputPath)
{
    if (isWaylandSession()) {
        return failureResult(
            {false,
             "Standard Ubuntu 22.04 Wayland portals cannot capture a window selected by "
             "programmatic ID or name. Use full capture or log in to Ubuntu on Xorg."});
    }
    std::vector<WindowRecord> windows;
    const WindowRecord* selected = nullptr;
    OperationResult result = resolveWindow(idOrName, windows, selected);
    if (!result.success) {
        return failureResult(result);
    }
    std::filesystem::path resolvedPath;
    result = resolveOutputPath(outputPath, "window", resolvedPath);
    if (!result.success) {
        return failureResult(result);
    }
    return captureX11Drawable(selected->handle, 0, 0, selected->info.width,
                              selected->info.height, resolvedPath);
}

std::string ScreenCapturePrivate::lowerAscii(const std::string& value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

bool ScreenCapturePrivate::isSupportedImageExtension(const std::filesystem::path& path)
{
    const std::string extension = lowerAscii(path.extension().string());
    return extension == ".jpg" || extension == ".jpeg" || extension == ".bmp";
}

std::string ScreenCapturePrivate::processName(std::uint32_t processId)
{
    if (processId == 0) {
        return {};
    }
    std::ifstream stream("/proc/" + std::to_string(processId) + "/comm");
    std::string name;
    std::getline(stream, name);
    return name;
}

std::uint8_t ScreenCapturePrivate::extractColor(unsigned long pixel, unsigned long mask)
{
    if (mask == 0) {
        return 0;
    }
    unsigned int shift = 0;
    while (((mask >> shift) & 1UL) == 0UL) {
        ++shift;
    }
    const unsigned long maximum = mask >> shift;
    const unsigned long value = (pixel & mask) >> shift;
    return static_cast<std::uint8_t>(value * 255UL / maximum);
}

int ScreenCapturePrivate::handleX11Error(Display*, XErrorEvent*)
{
    x11ErrorOccurred_ = true;
    return 0;
}

ScreenshotResult ScreenCapturePrivate::failureResult(OperationResult operation)
{
    ScreenshotResult result;
    result.operation = std::move(operation);
    return result;
}

bool ScreenCapturePrivate::isWaylandSession() const
{
    return LinuxSessionClient::isWaylandSession();
}

OperationResult ScreenCapturePrivate::ensureX11()
{
    if (display_ != nullptr) {
        return {true, {}};
    }
    display_ = XOpenDisplay(nullptr);
    if (display_ == nullptr) {
        return {false, "Unable to open the X11 display."};
    }
    root_ = DefaultRootWindow(display_);
    clientListAtom_ = XInternAtom(display_, "_NET_CLIENT_LIST", False);
    activeWindowAtom_ = XInternAtom(display_, "_NET_ACTIVE_WINDOW", False);
    windowNameAtom_ = XInternAtom(display_, "_NET_WM_NAME", False);
    utf8StringAtom_ = XInternAtom(display_, "UTF8_STRING", False);
    processIdAtom_ = XInternAtom(display_, "_NET_WM_PID", False);
    windowStateAtom_ = XInternAtom(display_, "_NET_WM_STATE", False);
    hiddenStateAtom_ = XInternAtom(display_, "_NET_WM_STATE_HIDDEN", False);
    return {true, {}};
}

OperationResult ScreenCapturePrivate::discoverWindows(std::vector<WindowRecord>& windows)
{
    windows.clear();
    OperationResult result = ensureX11();
    if (!result.success) {
        return result;
    }
    std::vector<unsigned long> handles = readCardinalArray(root_, clientListAtom_);
    if (handles.empty()) {
        Window rootReturn = None;
        Window parentReturn = None;
        Window* children = nullptr;
        unsigned int count = 0;
        if (XQueryTree(display_, root_, &rootReturn, &parentReturn, &children, &count) == 0) {
            return {false, "Unable to enumerate X11 windows."};
        }
        handles.reserve(count);
        for (unsigned int index = 0; index < count; ++index) {
            handles.push_back(static_cast<unsigned long>(children[index]));
        }
        if (children != nullptr) {
            XFree(children);
        }
    }

    unsigned long activeWindow = 0;
    readCardinal(root_, activeWindowAtom_, activeWindow);
    for (unsigned long rawHandle : handles) {
        const Window handle = static_cast<Window>(rawHandle);
        const std::string title = readWindowTitle(handle);
        if (title.empty()) {
            continue;
        }
        XWindowAttributes attributes{};
        if (XGetWindowAttributes(display_, handle, &attributes) == 0
            || attributes.width <= 0 || attributes.height <= 0
            || attributes.override_redirect != False) {
            continue;
        }
        Window child = None;
        int rootX = 0;
        int rootY = 0;
        XTranslateCoordinates(display_, handle, root_, 0, 0, &rootX, &rootY, &child);
        unsigned long processId = 0;
        readCardinal(handle, processIdAtom_, processId);

        WindowRecord record;
        record.handle = handle;
        std::ostringstream id;
        id << "0x" << std::hex << static_cast<unsigned long>(handle);
        record.info.id = id.str();
        record.info.title = title;
        record.info.processId = processId <= std::numeric_limits<std::uint32_t>::max()
            ? static_cast<std::uint32_t>(processId)
            : 0U;
        record.info.processName = processName(record.info.processId);
        record.info.minimized = hasWindowState(handle, hiddenStateAtom_)
            || attributes.map_state == IsUnmapped;
        record.info.active = rawHandle == activeWindow;
        record.info.x = rootX;
        record.info.y = rootY;
        record.info.width = attributes.width;
        record.info.height = attributes.height;
        windows.push_back(std::move(record));
    }
    std::sort(windows.begin(), windows.end(), [](const WindowRecord& left,
                                                 const WindowRecord& right) {
        return lowerAscii(left.info.title) < lowerAscii(right.info.title);
    });
    return {true, {}};
}

OperationResult ScreenCapturePrivate::resolveWindow(const std::string& idOrName,
                                                     std::vector<WindowRecord>& windows,
                                                     const WindowRecord*& selected)
{
    selected = nullptr;
    if (idOrName.empty()) {
        return {false, "Window ID or name cannot be empty."};
    }
    OperationResult result = discoverWindows(windows);
    if (!result.success) {
        return result;
    }
    for (const WindowRecord& window : windows) {
        if (window.info.id == idOrName) {
            selected = &window;
            return {true, {}};
        }
    }
    for (const WindowRecord& window : windows) {
        if (window.info.title == idOrName || window.info.processName == idOrName) {
            if (selected != nullptr) {
                return {false, "Window target is ambiguous; use an ID from window list."};
            }
            selected = &window;
        }
    }
    return selected == nullptr ? OperationResult{false, "Window was not found: " + idOrName}
                               : OperationResult{true, {}};
}

bool ScreenCapturePrivate::readCardinal(Window window, Atom property, unsigned long& value) const
{
    value = 0;
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesRemaining = 0;
    unsigned char* data = nullptr;
    const int status = XGetWindowProperty(display_, window, property, 0, 1, False,
                                          AnyPropertyType, &actualType, &actualFormat,
                                          &itemCount, &bytesRemaining, &data);
    if (status != Success || data == nullptr || itemCount == 0 || actualFormat != 32) {
        if (data != nullptr) {
            XFree(data);
        }
        return false;
    }
    value = *reinterpret_cast<unsigned long*>(data);
    XFree(data);
    return true;
}

std::vector<unsigned long> ScreenCapturePrivate::readCardinalArray(Window window,
                                                                    Atom property) const
{
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesRemaining = 0;
    unsigned char* data = nullptr;
    const int status = XGetWindowProperty(display_, window, property, 0, 1024 * 1024, False,
                                          AnyPropertyType, &actualType, &actualFormat,
                                          &itemCount, &bytesRemaining, &data);
    std::vector<unsigned long> result;
    if (status == Success && data != nullptr && actualFormat == 32) {
        const auto* values = reinterpret_cast<const unsigned long*>(data);
        result.assign(values, values + itemCount);
    }
    if (data != nullptr) {
        XFree(data);
    }
    return result;
}

std::string ScreenCapturePrivate::readWindowTitle(Window window) const
{
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesRemaining = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(display_, window, windowNameAtom_, 0, 1024 * 1024, False,
                           utf8StringAtom_, &actualType, &actualFormat, &itemCount,
                           &bytesRemaining, &data) == Success
        && data != nullptr && actualFormat == 8) {
        std::string title(reinterpret_cast<char*>(data), itemCount);
        XFree(data);
        return title;
    }
    if (data != nullptr) {
        XFree(data);
    }
    char* legacyTitle = nullptr;
    if (XFetchName(display_, window, &legacyTitle) != 0 && legacyTitle != nullptr) {
        const std::string title(legacyTitle);
        XFree(legacyTitle);
        return title;
    }
    return {};
}

bool ScreenCapturePrivate::hasWindowState(Window window, Atom state) const
{
    const std::vector<unsigned long> states = readCardinalArray(window, windowStateAtom_);
    return std::find(states.begin(), states.end(), static_cast<unsigned long>(state))
        != states.end();
}

OperationResult ScreenCapturePrivate::resolveDefaultOutputDirectory(
    std::filesystem::path& directory)
{
    if (!_defaultOutputDirectory) {
        std::error_code error;
        std::filesystem::path temporary = std::filesystem::temp_directory_path(error);
        if (error) {
            return {false, "Unable to resolve the Linux temporary directory: "
                               + error.message() + "."};
        }
        temporary /= "DesktopCTL";
        temporary /= "screenshots";
        std::filesystem::create_directories(temporary, error);
        if (error) {
            return {false, "Unable to create the default screenshot directory: "
                               + error.message() + "."};
        }
        _defaultOutputDirectory = std::filesystem::absolute(temporary, error);
        if (error) {
            return {false, "Unable to resolve the screenshot directory: " + error.message()
                               + "."};
        }
    }
    directory = *_defaultOutputDirectory;
    return {true, {}};
}

OperationResult ScreenCapturePrivate::resolveOutputPath(
    const std::optional<std::string>& requestedPath, const std::string& prefix,
    std::filesystem::path& outputPath)
{
    std::error_code error;
    if (requestedPath && !requestedPath->empty()) {
        outputPath = std::filesystem::u8path(*requestedPath);
        if (!isSupportedImageExtension(outputPath)) {
            return {false, "Output extension must be .jpg, .jpeg, or .bmp."};
        }
        outputPath = std::filesystem::absolute(outputPath, error);
        if (error) {
            return {false, "Unable to resolve the screenshot output path: " + error.message()
                               + "."};
        }
        const std::filesystem::path parent = outputPath.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, error);
            if (error) {
                return {false, "Unable to create the screenshot output directory: "
                                   + error.message() + "."};
            }
        }
        return {true, {}};
    }

    std::filesystem::path directory;
    OperationResult result = resolveDefaultOutputDirectory(directory);
    if (!result.success) {
        return result;
    }
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&time, &local);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::ostringstream filename;
    filename << prefix << '_' << std::put_time(&local, "%Y%m%d_%H%M%S_")
             << std::setw(3) << std::setfill('0') << milliseconds.count() << ".jpg";
    outputPath = directory / filename.str();
    return {true, {}};
}

ScreenshotResult ScreenCapturePrivate::captureX11Drawable(
    Drawable drawable, int x, int y, int width, int height,
    const std::filesystem::path& outputPath)
{
    if (width <= 0 || height <= 0) {
        return failureResult({false, "The X11 capture dimensions are invalid."});
    }
    XSync(display_, False);
    x11ErrorOccurred_ = false;
    const XErrorHandler previousHandler = XSetErrorHandler(
        &ScreenCapturePrivate::handleX11Error);
    XImage* image = XGetImage(display_, drawable, x, y, static_cast<unsigned int>(width),
                              static_cast<unsigned int>(height), AllPlanes, ZPixmap);
    XSync(display_, False);
    XSetErrorHandler(previousHandler);
    if (image == nullptr || x11ErrorOccurred_) {
        if (image != nullptr) {
            XDestroyImage(image);
        }
        return failureResult(
            {false, "XGetImage failed. The window may be minimized, protected, or unavailable "
                    "through XWayland."});
    }
    LinuxRgbImage rgb;
    rgb.width = width;
    rgb.height = height;
    rgb.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U);
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            const unsigned long pixel = XGetPixel(image, column, row);
            const std::size_t offset = (static_cast<std::size_t>(row)
                                        * static_cast<std::size_t>(width)
                                        + static_cast<std::size_t>(column)) * 3U;
            rgb.pixels[offset] = extractColor(pixel, image->red_mask);
            rgb.pixels[offset + 1U] = extractColor(pixel, image->green_mask);
            rgb.pixels[offset + 2U] = extractColor(pixel, image->blue_mask);
        }
    }
    XDestroyImage(image);
    OperationResult writeResult = LinuxImageCodec::write(outputPath, rgb);
    return writeResult.success ? createScreenshotResult(outputPath)
                               : failureResult(writeResult);
}

ScreenshotResult ScreenCapturePrivate::captureWaylandDesktop(
    const std::filesystem::path& outputPath)
{
    const std::string path = outputPath.u8string();
    if (path.find('\n') != std::string::npos || path.find('\r') != std::string::npos) {
        return failureResult(
            {false, "A Wayland screenshot output path cannot contain a line break."});
    }
    OperationResult captureResult = LinuxSessionClient::request("SCREENSHOT " + path);
    return captureResult.success ? createScreenshotResult(outputPath)
                                 : failureResult(captureResult);
}

ScreenshotResult ScreenCapturePrivate::createScreenshotResult(
    const std::filesystem::path& outputPath)
{
    ScreenshotResult result;
    result.operation = {true, {}};
    result.path = outputPath.u8string();
    result.directory = outputPath.parent_path().u8string();

    std::uint64_t count = 0;
    std::error_code error;
    for (std::filesystem::directory_iterator iterator(outputPath.parent_path(), error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (iterator->is_regular_file(error) && !error
            && isSupportedImageExtension(iterator->path())) {
            ++count;
        }
    }
    if (!error) {
        result.imageCount = count;
        result.cleanupRecommended = count > 50U;
        result.message = "Screenshot saved. The output directory contains "
            + std::to_string(count) + " image file(s).";
        if (result.cleanupRecommended) {
            result.message += " More than 50 images are present; remove screenshots you no "
                              "longer need.";
        }
    } else {
        result.message = "Screenshot saved, but the output directory could not be counted: "
            + error.message() + ".";
    }
    return result;
}

} // namespace desktopctl
