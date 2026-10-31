#include "mcpToolDispatcher.h"

#include "jsonCodec.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace desktopctl {

JsonValue McpToolDispatcher::toolDefinitions() const
{
    const auto stringProperty = [](const std::string& description) {
        return JsonValue(JsonValue::Object{
            {"type", JsonValue("string")},
            {"description", JsonValue(description)}
        });
    };
    const auto integerProperty = [](const std::string& description) {
        return JsonValue(JsonValue::Object{
            {"type", JsonValue("integer")},
            {"description", JsonValue(description)}
        });
    };
    const auto booleanProperty = [](const std::string& description) {
        return JsonValue(JsonValue::Object{
            {"type", JsonValue("boolean")},
            {"description", JsonValue(description)}
        });
    };
    const auto enumProperty = [](JsonValue::Array values, const std::string& description) {
        return JsonValue(JsonValue::Object{
            {"type", JsonValue("string")},
            {"enum", JsonValue(std::move(values))},
            {"description", JsonValue(description)}
        });
    };
    const auto schema = [](JsonValue::Object properties, JsonValue::Array required) {
        return JsonValue(JsonValue::Object{
            {"type", JsonValue("object")},
            {"properties", JsonValue(std::move(properties))},
            {"required", JsonValue(std::move(required))},
            {"additionalProperties", JsonValue(false)}
        });
    };
    const auto tool = [](const std::string& name, const std::string& description,
                         JsonValue inputSchema, bool readOnly) {
        return JsonValue(JsonValue::Object{
            {"name", JsonValue(name)},
            {"description", JsonValue(description)},
            {"inputSchema", std::move(inputSchema)},
            {"annotations", JsonValue(JsonValue::Object{
                {"readOnlyHint", JsonValue(readOnly)},
                {"destructiveHint", JsonValue(false)},
                {"openWorldHint", JsonValue(false)}
            })}
        });
    };

    JsonValue::Array tools;
    tools.push_back(tool(
        "session_start",
        "Start the required green activity indicator. Call this before every other desktop tool. "
        "The default session closes automatically after three minutes without a tool call. Set "
        "keep_alive only for an explicitly unattended session that must remain active.",
        schema({
            {"keep_alive", booleanProperty(
                "True disables the inactivity timeout until session_stop, logout, reboot, or "
                "session failure. Defaults to false.")}
        }, {}), false));
    tools.push_back(tool(
        "session_stop",
        "Stop the desktop control session and immediately remove all green overlays.",
        schema({}, {}), false));
    tools.push_back(tool(
        "mouse_control",
        "Move, click, press, release, or scroll the mouse. Absolute coordinates are physical "
        "pixels on the virtual desktop and may be negative on multi-monitor systems.",
        schema({
            {"action", enumProperty({JsonValue("move"), JsonValue("move_relative"),
                 JsonValue("click"), JsonValue("down"), JsonValue("up"), JsonValue("wheel")},
                 "Mouse operation to perform.")},
            {"x", integerProperty("Absolute x coordinate for move, or delta x for move_relative.")},
            {"y", integerProperty("Absolute y coordinate for move, or delta y for move_relative.")},
            {"button", enumProperty({JsonValue("left"), JsonValue("right"), JsonValue("middle")},
                 "Button used by click, down, or up.")},
            {"count", integerProperty("Click count from 1 through 100. Defaults to 1.")},
            {"delta", integerProperty("Wheel delta. A magnitude of 120 is one notch.")},
            {"direction", enumProperty({JsonValue("vertical"), JsonValue("horizontal")},
                 "Wheel direction. Defaults to vertical.")}
        }, {JsonValue("action")}), false));
    tools.push_back(tool(
        "keyboard_control",
        "Press, hold, release, combine, or type keyboard input into the active window. Activate "
        "the intended window before sending input.",
        schema({
            {"action", enumProperty({JsonValue("press"), JsonValue("down"), JsonValue("up"),
                 JsonValue("hotkey"), JsonValue("type"), JsonValue("type_physical"),
                 JsonValue("type_unicode")}, "Keyboard operation to perform.")},
            {"key", stringProperty("Portable key name used by press, down, or up.")},
            {"keys", JsonValue(JsonValue::Object{
                {"type", JsonValue("array")},
                {"items", JsonValue(JsonValue::Object{{"type", JsonValue("string")}})},
                {"minItems", JsonValue(static_cast<std::int64_t>(1))},
                {"description", JsonValue(
                    "Ordered key names used by hotkey. Supply separate names or a plus-delimited "
                    "expression such as ctrl+shift+escape. Hotkeys follow the active keyboard "
                    "layout; use type or type_physical for literal symbols and passwords.")}
            })},
            {"text", stringProperty(
                "UTF-8 text used by type, type_physical, or type_unicode. Hybrid type prefers "
                "physical scan codes and falls back to Unicode. This does not press Enter.")}
        }, {JsonValue("action")}), false));
    tools.push_back(tool(
        "screenshot_capture",
        "Capture the full desktop, a coordinate region, or one application window when supported "
        "by the active display protocol. The default is a quality-95 JPG in DesktopCTL/screenshots "
        "under the current user's system temporary directory. The result includes the absolute "
        "path, image count, and cleanup guidance.",
        schema({
            {"mode", enumProperty({JsonValue("full"), JsonValue("region"), JsonValue("window")},
                 "Capture mode. Defaults to full.")},
            {"x", integerProperty("Region x coordinate in physical virtual-desktop pixels.")},
            {"y", integerProperty("Region y coordinate in physical virtual-desktop pixels.")},
            {"width", integerProperty("Positive region width in physical pixels.")},
            {"height", integerProperty("Positive region height in physical pixels.")},
            {"target", stringProperty("Window ID, exact title, or unique process name.")},
            {"output", stringProperty("Optional JPG, JPEG, or BMP output path. Relative paths are "
                 "resolved from the MCP process working directory.")}
        }, {}), false));
    tools.push_back(tool(
        "window_list",
        "List visible titled top-level windows with transient IDs, process data, active state, "
        "and bounds. Supported on Windows and Xorg; standard Wayland does not expose a global "
        "window list.",
        schema({}, {}), true));
    tools.push_back(tool(
        "window_activate",
        "Restore and activate a window by transient ID, exact title, or unique process name on "
        "Windows or Xorg. Standard Wayland does not permit arbitrary forced activation.",
        schema({{"target", stringProperty("Window ID, exact title, or unique process name.")}},
               {JsonValue("target")}), false));
    tools.push_back(tool(
        "app_list",
        "List applications available to the current user from the Windows AppsFolder or Linux "
        "desktop application catalog.",
        schema({}, {}), true));
    tools.push_back(tool(
        "app_open",
        "Open an application by a stable ID returned by app_list or by a unique exact display name.",
        schema({{"target", stringProperty("Stable application ID or unique exact display name.")}},
               {JsonValue("target")}), false));
    return JsonValue(std::move(tools));
}

JsonValue McpToolDispatcher::call(const std::string& name, const JsonValue& arguments)
{
    if (!isKnownTool(name)) {
        return errorResult("Unknown desktopctl tool: " + name);
    }
    if (!arguments.isObject()) {
        return errorResult("Tool arguments must be a JSON object.");
    }
    if (name == "session_start") {
        bool keepAlive = false;
        std::string error;
        if (!getBoolean(arguments, "keep_alive", false, keepAlive, error)) {
            return errorResult(error);
        }
        const OperationResult result = activityIndicator_.startBackground(keepAlive);
        return toolResult(operationPayload(result), !result.success);
    }

    if (!activityIndicator_.isRunning()) {
        return errorResult("Session is not started. Call session_start before using other tools.");
    }
    const OperationResult activityResult = activityIndicator_.recordActivity();
    if (!activityResult.success) {
        return errorResult("Session is not started. Call session_start before using other tools.");
    }

    if (name == "session_stop") {
        const OperationResult result = activityIndicator_.requestStop();
        return toolResult(operationPayload(result), !result.success);
    }
    if (name == "mouse_control") {
        return callMouse(arguments);
    }
    if (name == "keyboard_control") {
        return callKeyboard(arguments);
    }
    if (name == "screenshot_capture") {
        return callScreenshot(arguments);
    }
    if (name == "window_list") {
        return callWindowList();
    }
    if (name == "window_activate") {
        std::string target;
        std::string error;
        if (!getString(arguments, "target", true, target, error)) {
            return errorResult(error);
        }
        const OperationResult result = screenCapture_.activateWindow(target);
        return toolResult(operationPayload(result), !result.success);
    }
    if (name == "app_list") {
        return callApplicationList();
    }

    std::string target;
    std::string error;
    if (!getString(arguments, "target", true, target, error)) {
        return errorResult(error);
    }
    const OperationResult result = applicationCatalog_.openApplication(target);
    return toolResult(operationPayload(result), !result.success);
}

void McpToolDispatcher::shutdown()
{
    if (activityIndicator_.isRunning()) {
        activityIndicator_.requestStop();
    }
}

JsonValue McpToolDispatcher::operationPayload(const OperationResult& result)
{
    return JsonValue(JsonValue::Object{
        {"success", JsonValue(result.success)},
        {"message", JsonValue(result.message)}
    });
}

JsonValue McpToolDispatcher::screenshotPayload(const ScreenshotResult& result)
{
    JsonValue imageCount;
    if (result.imageCount) {
        imageCount = JsonValue(*result.imageCount);
    }
    return JsonValue(JsonValue::Object{
        {"success", JsonValue(result.operation.success)},
        {"path", result.operation.success ? JsonValue(result.path) : JsonValue()},
        {"directory", result.operation.success ? JsonValue(result.directory) : JsonValue()},
        {"imageCount", std::move(imageCount)},
        {"cleanupRecommended", JsonValue(result.cleanupRecommended)},
        {"message", JsonValue(result.operation.success ? result.message : result.operation.message)}
    });
}

JsonValue McpToolDispatcher::toolResult(JsonValue payload, bool isError)
{
    const std::string text = JsonCodec::serialize(payload);
    return JsonValue(JsonValue::Object{
        {"content", JsonValue(JsonValue::Array{
            JsonValue(JsonValue::Object{
                {"type", JsonValue("text")},
                {"text", JsonValue(text)}
            })
        })},
        {"structuredContent", std::move(payload)},
        {"isError", JsonValue(isError)}
    });
}

JsonValue McpToolDispatcher::errorResult(const std::string& message)
{
    return toolResult(JsonValue(JsonValue::Object{
        {"success", JsonValue(false)},
        {"message", JsonValue(message)}
    }), true);
}

bool McpToolDispatcher::getString(const JsonValue& arguments, const std::string& key,
                                  bool required, std::string& value, std::string& error)
{
    const JsonValue* member = arguments.find(key);
    if (member == nullptr) {
        value.clear();
        if (!required) {
            return true;
        }
        error = "Missing required string argument: " + key;
        return false;
    }
    if (!member->isString()) {
        error = "Argument must be a string: " + key;
        return false;
    }
    value = member->string();
    if (required && value.empty()) {
        error = "Argument cannot be empty: " + key;
        return false;
    }
    return true;
}

bool McpToolDispatcher::getInteger(const JsonValue& arguments, const std::string& key,
                                   bool required, int& value, std::string& error)
{
    const JsonValue* member = arguments.find(key);
    if (member == nullptr) {
        if (!required) {
            return true;
        }
        error = "Missing required integer argument: " + key;
        return false;
    }
    const std::optional<std::int64_t> integer = member->integer();
    if (!integer || *integer < std::numeric_limits<int>::min()
        || *integer > std::numeric_limits<int>::max()) {
        error = "Argument must be a 32-bit integer: " + key;
        return false;
    }
    value = static_cast<int>(*integer);
    return true;
}

bool McpToolDispatcher::getBoolean(const JsonValue& arguments, const std::string& key,
                                   bool required, bool& value, std::string& error)
{
    const JsonValue* member = arguments.find(key);
    if (member == nullptr) {
        value = false;
        if (!required) {
            return true;
        }
        error = "Missing required boolean argument: " + key;
        return false;
    }
    if (!member->isBoolean()) {
        error = "Argument must be a boolean: " + key;
        return false;
    }
    value = member->boolean();
    return true;
}

bool McpToolDispatcher::getStringArray(const JsonValue& arguments, const std::string& key,
                                       std::vector<std::string>& value, std::string& error)
{
    const JsonValue* member = arguments.find(key);
    if (member == nullptr || !member->isArray() || member->array().empty()) {
        error = "Argument must be a non-empty string array: " + key;
        return false;
    }
    value.clear();
    value.reserve(member->array().size());
    for (const JsonValue& item : member->array()) {
        if (!item.isString() || item.string().empty()) {
            error = "Argument must contain only non-empty strings: " + key;
            return false;
        }
        value.push_back(item.string());
    }
    return true;
}

bool McpToolDispatcher::isKnownTool(const std::string& name)
{
    static constexpr std::array<const char*, 9> ToolNames{
        "session_start", "session_stop", "mouse_control", "keyboard_control",
        "screenshot_capture", "window_list", "window_activate", "app_list", "app_open"
    };
    return std::find(ToolNames.begin(), ToolNames.end(), name) != ToolNames.end();
}

JsonValue McpToolDispatcher::callMouse(const JsonValue& arguments)
{
    std::string action;
    std::string error;
    if (!getString(arguments, "action", true, action, error)) {
        return errorResult(error);
    }
    if (action == "move" || action == "move_relative") {
        int x = 0;
        int y = 0;
        if (!getInteger(arguments, "x", true, x, error)
            || !getInteger(arguments, "y", true, y, error)) {
            return errorResult(error);
        }
        const OperationResult result = action == "move" ? automation_.moveMouse(x, y)
                                                         : automation_.moveMouseRelative(x, y);
        return toolResult(operationPayload(result), !result.success);
    }

    if (action == "click" || action == "down" || action == "up") {
        std::string buttonName;
        if (!getString(arguments, "button", true, buttonName, error)) {
            return errorResult(error);
        }
        MouseButton button = MouseButton::Left;
        if (buttonName == "right") {
            button = MouseButton::Right;
        } else if (buttonName == "middle") {
            button = MouseButton::Middle;
        } else if (buttonName != "left") {
            return errorResult("button must be left, right, or middle.");
        }
        OperationResult result;
        if (action == "click") {
            int count = 1;
            if (!getInteger(arguments, "count", false, count, error)) {
                return errorResult(error);
            }
            if (count < 1 || count > 100) {
                return errorResult("count must be from 1 through 100.");
            }
            result = automation_.clickMouse(button, count);
        } else {
            result = action == "down" ? automation_.mouseDown(button)
                                       : automation_.mouseUp(button);
        }
        return toolResult(operationPayload(result), !result.success);
    }

    if (action == "wheel") {
        int delta = 0;
        std::string direction;
        if (!getInteger(arguments, "delta", true, delta, error)
            || !getString(arguments, "direction", false, direction, error)) {
            return errorResult(error);
        }
        if (direction.empty()) {
            direction = "vertical";
        }
        if (direction != "vertical" && direction != "horizontal") {
            return errorResult("direction must be vertical or horizontal.");
        }
        const OperationResult result = automation_.scrollMouse(
            delta, direction == "vertical" ? WheelDirection::Vertical
                                            : WheelDirection::Horizontal);
        return toolResult(operationPayload(result), !result.success);
    }
    return errorResult("Unknown mouse action: " + action);
}

JsonValue McpToolDispatcher::callKeyboard(const JsonValue& arguments)
{
    std::string action;
    std::string error;
    if (!getString(arguments, "action", true, action, error)) {
        return errorResult(error);
    }
    if (action == "press" || action == "down" || action == "up") {
        std::string key;
        if (!getString(arguments, "key", true, key, error)) {
            return errorResult(error);
        }
        OperationResult result;
        if (action == "press") {
            result = automation_.pressKey(key);
        } else if (action == "down") {
            result = automation_.keyDown(key);
        } else {
            result = automation_.keyUp(key);
        }
        return toolResult(operationPayload(result), !result.success);
    }
    if (action == "hotkey") {
        std::vector<std::string> keys;
        if (!getStringArray(arguments, "keys", keys, error)) {
            return errorResult(error);
        }
        const OperationResult result = automation_.pressHotkey(keys);
        return toolResult(operationPayload(result), !result.success);
    }
    if (action == "type" || action == "type_physical" || action == "type_unicode") {
        std::string text;
        if (!getString(arguments, "text", true, text, error)) {
            return errorResult(error);
        }
        TextInputMode mode = TextInputMode::Hybrid;
        if (action == "type_physical") {
            mode = TextInputMode::Physical;
        } else if (action == "type_unicode") {
            mode = TextInputMode::Unicode;
        }
        const OperationResult result = automation_.typeText(text, mode);
        return toolResult(operationPayload(result), !result.success);
    }
    return errorResult("Unknown keyboard action: " + action);
}

JsonValue McpToolDispatcher::callScreenshot(const JsonValue& arguments)
{
    std::string mode;
    std::string output;
    std::string error;
    if (!getString(arguments, "mode", false, mode, error)
        || !getString(arguments, "output", false, output, error)) {
        return errorResult(error);
    }
    if (mode.empty()) {
        mode = "full";
    }
    const std::optional<std::string> outputPath = output.empty()
        ? std::nullopt
        : std::optional<std::string>(output);

    ScreenshotResult result;
    if (mode == "full") {
        result = screenCapture_.captureDesktop(outputPath);
    } else if (mode == "region") {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        if (!getInteger(arguments, "x", true, x, error)
            || !getInteger(arguments, "y", true, y, error)
            || !getInteger(arguments, "width", true, width, error)
            || !getInteger(arguments, "height", true, height, error)) {
            return errorResult(error);
        }
        if (width <= 0 || height <= 0) {
            return errorResult("width and height must be positive.");
        }
        result = screenCapture_.captureRegion(ScreenRegion{x, y, width, height}, outputPath);
    } else if (mode == "window") {
        std::string target;
        if (!getString(arguments, "target", true, target, error)) {
            return errorResult(error);
        }
        result = screenCapture_.captureWindow(target, outputPath);
    } else {
        return errorResult("mode must be full, region, or window.");
    }
    return toolResult(screenshotPayload(result), !result.operation.success);
}

JsonValue McpToolDispatcher::callWindowList()
{
    const WindowListResult listing = screenCapture_.listWindows();
    if (!listing.operation.success) {
        return toolResult(operationPayload(listing.operation), true);
    }
    JsonValue::Array windows;
    windows.reserve(listing.windows.size());
    for (const WindowInfo& window : listing.windows) {
        windows.push_back(JsonValue(JsonValue::Object{
            {"id", JsonValue(window.id)},
            {"title", JsonValue(window.title)},
            {"processId", JsonValue(static_cast<std::uint64_t>(window.processId))},
            {"processName", JsonValue(window.processName)},
            {"minimized", JsonValue(window.minimized)},
            {"active", JsonValue(window.active)},
            {"x", JsonValue(static_cast<std::int64_t>(window.x))},
            {"y", JsonValue(static_cast<std::int64_t>(window.y))},
            {"width", JsonValue(static_cast<std::int64_t>(window.width))},
            {"height", JsonValue(static_cast<std::int64_t>(window.height))}
        }));
    }
    return toolResult(JsonValue(JsonValue::Object{
        {"success", JsonValue(true)},
        {"windows", JsonValue(std::move(windows))}
    }), false);
}

JsonValue McpToolDispatcher::callApplicationList()
{
    const ApplicationListResult listing = applicationCatalog_.listApplications();
    if (!listing.operation.success) {
        return toolResult(operationPayload(listing.operation), true);
    }
    JsonValue::Array applications;
    applications.reserve(listing.applications.size());
    for (const ApplicationInfo& application : listing.applications) {
        applications.push_back(JsonValue(JsonValue::Object{
            {"id", JsonValue(application.id)},
            {"name", JsonValue(application.name)}
        }));
    }
    return toolResult(JsonValue(JsonValue::Object{
        {"success", JsonValue(true)},
        {"applications", JsonValue(std::move(applications))}
    }), false);
}

} // namespace desktopctl
