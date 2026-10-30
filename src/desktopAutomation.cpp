#include "desktopAutomation.h"

#include "private/desktopAutomationPrivate.h"

#include <utility>

namespace desktopctl {

DesktopAutomation::DesktopAutomation()
    : _d(std::make_unique<DesktopAutomationPrivate>())
{
}

DesktopAutomation::~DesktopAutomation() = default;

DesktopAutomation::DesktopAutomation(DesktopAutomation&&) noexcept = default;

DesktopAutomation& DesktopAutomation::operator=(DesktopAutomation&&) noexcept = default;

OperationResult DesktopAutomation::moveMouse(int x, int y)
{
    return _d->moveMouse(x, y);
}

OperationResult DesktopAutomation::moveMouseRelative(int deltaX, int deltaY)
{
    return _d->moveMouseRelative(deltaX, deltaY);
}

OperationResult DesktopAutomation::mouseDown(MouseButton button)
{
    return _d->mouseDown(button);
}

OperationResult DesktopAutomation::mouseUp(MouseButton button)
{
    return _d->mouseUp(button);
}

OperationResult DesktopAutomation::clickMouse(MouseButton button, int count)
{
    return _d->clickMouse(button, count);
}

OperationResult DesktopAutomation::scrollMouse(int delta, WheelDirection direction)
{
    return _d->scrollMouse(delta, direction);
}

OperationResult DesktopAutomation::keyDown(const std::string& key)
{
    return _d->keyDown(key);
}

OperationResult DesktopAutomation::keyUp(const std::string& key)
{
    return _d->keyUp(key);
}

OperationResult DesktopAutomation::pressKey(const std::string& key)
{
    return _d->pressKey(key);
}

OperationResult DesktopAutomation::pressHotkey(const std::vector<std::string>& keys)
{
    return _d->pressHotkey(keys);
}

OperationResult DesktopAutomation::typeText(const std::string& text)
{
    return _d->typeText(text);
}

} // namespace desktopctl
