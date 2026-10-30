#pragma once

#include "activityIndicator.h"
#include "applicationCatalog.h"
#include "desktopAutomation.h"
#include "screenCapture.h"

#include <optional>
#include <string>
#include <vector>

namespace desktopctl {

class CommandLineApplicationPrivate final {
public:
    int run(const std::vector<std::string>& arguments);

private:
    static std::string lowerCopy(const std::string& value);
    static std::optional<int> parseInteger(const std::string& value);
    static std::optional<MouseButton> parseMouseButton(const std::string& value);
    static std::string escapeJson(const std::string& value);
    static void printUsage();
    static int reportResult(const OperationResult& result);
    static int reportScreenshotResult(const ScreenshotResult& result);
    int runMouse(const std::vector<std::string>& arguments);
    int runKeyboard(const std::vector<std::string>& arguments);
    int runScreenshot(const std::vector<std::string>& arguments);
    int runSession(const std::vector<std::string>& arguments);
    int runApplication(const std::vector<std::string>& arguments);
    int runWindow(const std::vector<std::string>& arguments);

    DesktopAutomation automation_;
    ActivityIndicator activityIndicator_;
    ApplicationCatalog applicationCatalog_;
    ScreenCapture screenCapture_;
};

} // namespace desktopctl
