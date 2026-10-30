#include "private/screenCapturePrivate.h"

namespace desktopctl {

ScreenCapturePrivate::ScreenCapturePrivate() = default;

ScreenCapturePrivate::~ScreenCapturePrivate() = default;

WindowListResult ScreenCapturePrivate::listWindows()
{
    return {{false, "Window discovery is not implemented on Linux yet."}, {}};
}

OperationResult ScreenCapturePrivate::activateWindow(const std::string&)
{
    return {false, "Window activation is not implemented on Linux yet."};
}

ScreenshotResult ScreenCapturePrivate::captureDesktop(const std::optional<std::string>&)
{
    return {{false, "Screen capture is not implemented on Linux yet."}};
}

ScreenshotResult ScreenCapturePrivate::captureRegion(
    const ScreenRegion&, const std::optional<std::string>&)
{
    return {{false, "Screen capture is not implemented on Linux yet."}};
}

ScreenshotResult ScreenCapturePrivate::captureWindow(
    const std::string&, const std::optional<std::string>&)
{
    return {{false, "Window capture is not implemented on Linux yet."}};
}

} // namespace desktopctl
