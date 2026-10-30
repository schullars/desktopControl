#include "private/desktopAutomationPrivate.h"

namespace desktopctl {

DesktopAutomationPrivate::DesktopAutomationPrivate() = default;

DesktopAutomationPrivate::~DesktopAutomationPrivate() = default;

OperationResult DesktopAutomationPrivate::moveMouse(int, int)
{
    return {false, "Mouse control is not implemented on Linux yet."};
}

OperationResult DesktopAutomationPrivate::moveMouseRelative(int, int)
{
    return {false, "Mouse control is not implemented on Linux yet."};
}

OperationResult DesktopAutomationPrivate::mouseDown(MouseButton)
{
    return {false, "Mouse control is not implemented on Linux yet."};
}

OperationResult DesktopAutomationPrivate::mouseUp(MouseButton)
{
    return {false, "Mouse control is not implemented on Linux yet."};
}

OperationResult DesktopAutomationPrivate::clickMouse(MouseButton, int)
{
    return {false, "Mouse control is not implemented on Linux yet."};
}

OperationResult DesktopAutomationPrivate::scrollMouse(int, WheelDirection)
{
    return {false, "Mouse control is not implemented on Linux yet."};
}

OperationResult DesktopAutomationPrivate::keyDown(const std::string&)
{
    return {false, "Keyboard control is not implemented on Linux yet."};
}

OperationResult DesktopAutomationPrivate::keyUp(const std::string&)
{
    return {false, "Keyboard control is not implemented on Linux yet."};
}

OperationResult DesktopAutomationPrivate::pressKey(const std::string&)
{
    return {false, "Keyboard control is not implemented on Linux yet."};
}

OperationResult DesktopAutomationPrivate::pressHotkey(const std::vector<std::string>&)
{
    return {false, "Keyboard control is not implemented on Linux yet."};
}

OperationResult DesktopAutomationPrivate::typeText(const std::string&)
{
    return {false, "Keyboard control is not implemented on Linux yet."};
}

} // namespace desktopctl
