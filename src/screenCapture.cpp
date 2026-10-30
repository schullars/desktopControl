#include "screenCapture.h"

#include "private/screenCapturePrivate.h"

#include <memory>

namespace desktopctl {

ScreenCapture::ScreenCapture()
    : _d(std::make_unique<ScreenCapturePrivate>())
{
}

ScreenCapture::~ScreenCapture() = default;

WindowListResult ScreenCapture::listWindows()
{
    return _d->listWindows();
}

OperationResult ScreenCapture::activateWindow(const std::string& idOrName)
{
    return _d->activateWindow(idOrName);
}

ScreenshotResult ScreenCapture::captureDesktop(const std::optional<std::string>& outputPath)
{
    return _d->captureDesktop(outputPath);
}

ScreenshotResult ScreenCapture::captureRegion(
    const ScreenRegion& region, const std::optional<std::string>& outputPath)
{
    return _d->captureRegion(region, outputPath);
}

ScreenshotResult ScreenCapture::captureWindow(
    const std::string& idOrName, const std::optional<std::string>& outputPath)
{
    return _d->captureWindow(idOrName, outputPath);
}

} // namespace desktopctl
