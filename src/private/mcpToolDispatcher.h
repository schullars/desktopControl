#pragma once

#include "activityIndicator.h"
#include "applicationCatalog.h"
#include "desktopAutomation.h"
#include "jsonValue.h"
#include "screenCapture.h"

#include <string>

namespace desktopctl {

class McpToolDispatcher final {
public:
    JsonValue toolDefinitions() const;
    JsonValue call(const std::string& name, const JsonValue& arguments);
    void shutdown();

private:
    static JsonValue operationPayload(const OperationResult& result);
    static JsonValue screenshotPayload(const ScreenshotResult& result);
    static JsonValue toolResult(JsonValue payload, bool isError);
    static JsonValue errorResult(const std::string& message);
    static bool getString(const JsonValue& arguments, const std::string& key, bool required,
                          std::string& value, std::string& error);
    static bool getInteger(const JsonValue& arguments, const std::string& key, bool required,
                           int& value, std::string& error);
    static bool getStringArray(const JsonValue& arguments, const std::string& key,
                              std::vector<std::string>& value, std::string& error);
    static bool isKnownTool(const std::string& name);

    JsonValue callMouse(const JsonValue& arguments);
    JsonValue callKeyboard(const JsonValue& arguments);
    JsonValue callScreenshot(const JsonValue& arguments);
    JsonValue callWindowList();
    JsonValue callApplicationList();

    DesktopAutomation automation_;
    ActivityIndicator activityIndicator_;
    ApplicationCatalog applicationCatalog_;
    ScreenCapture screenCapture_;
};

} // namespace desktopctl
