#pragma once

#include "operationResult.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace desktopctl {

struct ScreenRegion {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct WindowInfo {
    std::string id;
    std::string title;
    std::uint32_t processId = 0;
    std::string processName;
    bool minimized = false;
    bool active = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct WindowListResult {
    OperationResult operation;
    std::vector<WindowInfo> windows;
};

struct ScreenshotResult {
    /// The capture operation status.
    OperationResult operation;
    /// The absolute UTF-8 path. Filesystem characters are preserved exactly.
    std::string path;
    /// The absolute UTF-8 parent directory. Filesystem characters are preserved exactly.
    std::string directory;
    /// The number of supported image files in the output directory, or no value on failure.
    std::optional<std::uint64_t> imageCount;
    /// True when imageCount is available and greater than 50.
    bool cleanupRecommended = false;
    /// A stable English status and cleanup message.
    std::string message;
};

class ScreenCapturePrivate;

class ScreenCapture final {
public:
    /// <summary>
    /// Creates the screen capture service for the current platform.
    /// </summary>
    ScreenCapture();
    ~ScreenCapture();

    ScreenCapture(const ScreenCapture&) = delete;
    ScreenCapture& operator=(const ScreenCapture&) = delete;

    /// <summary>
    /// Lists visible titled top-level windows on the current interactive desktop.
    /// </summary>
    /// <returns>Status and transient window IDs, titles, and process information.</returns>
    WindowListResult listWindows();

    /// <summary>
    /// Restores and activates a top-level window, then verifies foreground ownership.
    /// </summary>
    /// <param name="idOrName">A window ID, exact title, or unique process name.</param>
    /// <returns>Failure when Windows does not grant foreground activation.</returns>
    OperationResult activateWindow(const std::string& idOrName);

    /// <summary>
    /// Captures the entire virtual desktop.
    /// </summary>
    /// <param name="outputPath">
    /// Optional JPEG or BMP path; empty uses the system temporary directory.
    /// </param>
    /// <returns>Absolute path, directory image count, and cleanup advice on success.</returns>
    ScreenshotResult captureDesktop(const std::optional<std::string>& outputPath = std::nullopt);

    /// <summary>
    /// Captures a physical-pixel region of the virtual desktop.
    /// </summary>
    /// <param name="region">A region fully contained within the virtual desktop.</param>
    /// <param name="outputPath">Optional JPEG or BMP path; empty uses the default directory.</param>
    /// <returns>Absolute path, directory image count, and cleanup advice on success.</returns>
    ScreenshotResult captureRegion(
        const ScreenRegion& region,
        const std::optional<std::string>& outputPath = std::nullopt);

    /// <summary>
    /// Captures a top-level window by ID, exact title, or unique process name.
    /// </summary>
    /// <param name="idOrName">A window ID, title, or process name with a unique match.</param>
    /// <param name="outputPath">Optional JPEG or BMP path; empty uses the default directory.</param>
    /// <returns>The result of a target-aware PrintWindow capture.</returns>
    ScreenshotResult captureWindow(
        const std::string& idOrName,
        const std::optional<std::string>& outputPath = std::nullopt);

private:
    std::unique_ptr<ScreenCapturePrivate> _d;
};

} // namespace desktopctl
