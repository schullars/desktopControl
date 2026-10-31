#pragma once

#include "screenCapture.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#endif

namespace desktopctl {

class ScreenCapturePrivate final {
public:
    ScreenCapturePrivate();
    ~ScreenCapturePrivate();

    WindowListResult listWindows();
    OperationResult activateWindow(const std::string& idOrName);
    ScreenshotResult captureDesktop(const std::optional<std::string>& outputPath);
    ScreenshotResult captureRegion(const ScreenRegion& region,
                                   const std::optional<std::string>& outputPath);
    ScreenshotResult captureWindow(const std::string& idOrName,
                                   const std::optional<std::string>& outputPath);

private:
#if defined(_WIN32)
    struct WindowRecord {
        WindowInfo info;
        HWND handle = nullptr;
        std::wstring title;
        std::wstring processName;
    };

    struct EnumerationContext {
        ScreenCapturePrivate* owner = nullptr;
        std::vector<WindowRecord>* windows = nullptr;
    };

    static BOOL CALLBACK enumerateWindow(HWND window, LPARAM context);
    static OperationResult windowsFailure(const std::string& operation);
    static bool isSupportedImageExtension(const std::filesystem::path& path);
    void collectWindow(HWND window, std::vector<WindowRecord>& windows);
    OperationResult discoverWindows(std::vector<WindowRecord>& windows);
    OperationResult resolveWindow(const std::string& idOrName,
                                  std::vector<WindowRecord>& windows,
                                  const WindowRecord*& selected);
    OperationResult requestWindowActivation(HWND window);
    OperationResult resolveDefaultOutputDirectory(std::filesystem::path& directory);
    OperationResult resolveOutputPath(const std::optional<std::string>& requestedPath,
                                      const std::wstring& prefix,
                                      std::filesystem::path& outputPath);
    ScreenshotResult captureDesktopRegion(const ScreenRegion& region,
                                          const std::filesystem::path& outputPath);
    ScreenshotResult captureWindowTarget(HWND window,
                                         const std::filesystem::path& outputPath);
    ScreenshotResult createScreenshotResult(const std::filesystem::path& outputPath);
    OperationResult readBitmap(HDC memoryDc, HBITMAP bitmap, int width, int height,
                               std::vector<std::uint8_t>& pixels);
    OperationResult writeImage(const std::filesystem::path& outputPath, int width, int height,
                               const std::vector<std::uint8_t>& pixels);
    OperationResult writeBmp(const std::filesystem::path& outputPath, int width, int height,
                             const std::vector<std::uint8_t>& pixels);
    OperationResult writeJpeg(const std::filesystem::path& outputPath, int width, int height,
                              const std::vector<std::uint8_t>& pixels);
    // Keep every default capture in this process bound to the same verified directory.
    std::optional<std::filesystem::path> _defaultOutputDirectory;
#elif defined(__linux__)
    struct WindowRecord {
        WindowInfo info;
        Window handle = None;
    };

    static std::string lowerAscii(const std::string& value);
    static bool isSupportedImageExtension(const std::filesystem::path& path);
    static std::string processName(std::uint32_t processId);
    static std::uint8_t extractColor(unsigned long pixel, unsigned long mask);
    static int handleX11Error(Display* display, XErrorEvent* event);
    static ScreenshotResult failureResult(OperationResult operation);
    bool isWaylandSession() const;
    OperationResult ensureX11();
    OperationResult discoverWindows(std::vector<WindowRecord>& windows);
    OperationResult resolveWindow(const std::string& idOrName,
                                  std::vector<WindowRecord>& windows,
                                  const WindowRecord*& selected);
    bool readCardinal(Window window, Atom property, unsigned long& value) const;
    std::vector<unsigned long> readCardinalArray(Window window, Atom property) const;
    std::string readWindowTitle(Window window) const;
    bool hasWindowState(Window window, Atom state) const;
    OperationResult resolveDefaultOutputDirectory(std::filesystem::path& directory);
    OperationResult resolveOutputPath(const std::optional<std::string>& requestedPath,
                                      const std::string& prefix,
                                      std::filesystem::path& outputPath);
    ScreenshotResult captureX11Drawable(Drawable drawable, int x, int y, int width, int height,
                                        const std::filesystem::path& outputPath);
    ScreenshotResult captureWaylandDesktop(const std::filesystem::path& outputPath);
    ScreenshotResult createScreenshotResult(const std::filesystem::path& outputPath);

    Display* display_ = nullptr;
    Window root_ = None;
    Atom clientListAtom_ = None;
    Atom activeWindowAtom_ = None;
    Atom windowNameAtom_ = None;
    Atom utf8StringAtom_ = None;
    Atom processIdAtom_ = None;
    Atom windowStateAtom_ = None;
    Atom hiddenStateAtom_ = None;
    inline static bool x11ErrorOccurred_ = false;
    std::optional<std::filesystem::path> _defaultOutputDirectory;
#endif
};

} // namespace desktopctl
