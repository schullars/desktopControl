#pragma once

#include "screenCapture.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
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
#endif
};

} // namespace desktopctl
