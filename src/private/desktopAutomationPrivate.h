#pragma once

#include "desktopAutomation.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace desktopctl {

class DesktopAutomationPrivate final {
public:
    DesktopAutomationPrivate();
    ~DesktopAutomationPrivate();

    OperationResult moveMouse(int x, int y);
    OperationResult moveMouseRelative(int deltaX, int deltaY);
    OperationResult mouseDown(MouseButton button);
    OperationResult mouseUp(MouseButton button);
    OperationResult clickMouse(MouseButton button, int count);
    OperationResult scrollMouse(int delta, WheelDirection direction);
    OperationResult keyDown(const std::string& key);
    OperationResult keyUp(const std::string& key);
    OperationResult pressKey(const std::string& key);
    OperationResult pressHotkey(const std::vector<std::string>& keys);
    OperationResult typeText(const std::string& text);
private:
#if defined(_WIN32)
    struct KeyDefinition {
        std::uint16_t virtualKey = 0;
        bool extended = false;
    };

    static std::string normalizeKey(const std::string& key);
    static std::optional<KeyDefinition> parseKey(const std::string& key);
    static OperationResult windowsFailure(const std::string& operation);
    static OperationResult sendMouseButton(MouseButton button, bool pressed);
    static OperationResult sendKey(const KeyDefinition& key, bool pressed);
#endif
};

} // namespace desktopctl
