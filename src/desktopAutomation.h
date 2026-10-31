#pragma once

#include "operationResult.h"

#include <memory>
#include <string>
#include <vector>

namespace desktopctl {

enum class MouseButton {
    Left,
    Right,
    Middle
};

enum class WheelDirection {
    Vertical,
    Horizontal
};

/// <summary>
/// Selects how text characters are converted into platform keyboard input events.
/// </summary>
enum class TextInputMode {
    /// Prefer physical scan codes and fall back to Unicode for unmapped characters.
    Hybrid,
    /// Require every character to be representable by the active keyboard layout.
    Physical,
    /// Send text through the platform Unicode input mechanism.
    Unicode
};

class DesktopAutomationPrivate;

class DesktopAutomation final {
public:
    /// <summary>
    /// Creates the desktop automation backend for the current platform.
    /// </summary>
    DesktopAutomation();
    ~DesktopAutomation();

    DesktopAutomation(const DesktopAutomation&) = delete;
    DesktopAutomation& operator=(const DesktopAutomation&) = delete;
    DesktopAutomation(DesktopAutomation&&) noexcept;
    DesktopAutomation& operator=(DesktopAutomation&&) noexcept;

    /// <summary>
    /// Moves the pointer to absolute desktop coordinates.
    /// </summary>
    /// <param name="x">The desktop x-coordinate, which may be negative on Xorg or Windows.</param>
    /// <param name="y">The desktop y-coordinate, which may be negative on Xorg or Windows.</param>
    /// <returns>The operation result, including platform error details on failure.</returns>
    OperationResult moveMouse(int x, int y);

    /// <summary>
    /// Moves the pointer relative to its current position.
    /// </summary>
    /// <param name="deltaX">Horizontal movement in pixels.</param>
    /// <param name="deltaY">Vertical movement in pixels.</param>
    /// <returns>The operation result, including platform error details on failure.</returns>
    OperationResult moveMouseRelative(int deltaX, int deltaY);

    /// <summary>
    /// Presses a mouse button without releasing it.
    /// </summary>
    /// <param name="button">The mouse button to press.</param>
    /// <returns>The operation result. The caller must release the button later.</returns>
    OperationResult mouseDown(MouseButton button);

    /// <summary>
    /// Releases a mouse button.
    /// </summary>
    /// <param name="button">The mouse button to release.</param>
    /// <returns>The operation result.</returns>
    OperationResult mouseUp(MouseButton button);

    /// <summary>
    /// Clicks a mouse button one or more times.
    /// </summary>
    /// <param name="button">The mouse button to click.</param>
    /// <param name="count">The positive number of clicks.</param>
    /// <returns>The operation result.</returns>
    OperationResult clickMouse(MouseButton button, int count);

    /// <summary>
    /// Scrolls the vertical or horizontal wheel.
    /// </summary>
    /// <param name="delta">The scroll amount. A magnitude of 120 represents one notch.</param>
    /// <param name="direction">The scroll direction.</param>
    /// <returns>The operation result.</returns>
    OperationResult scrollMouse(int delta, WheelDirection direction);

    /// <summary>
    /// Presses a key without releasing it.
    /// </summary>
    /// <param name="key">A portable key name such as ctrl, enter, a, or f12.</param>
    /// <returns>The operation result. The caller must release the key later.</returns>
    OperationResult keyDown(const std::string& key);

    /// <summary>
    /// Releases a key.
    /// </summary>
    /// <param name="key">A portable key name such as ctrl, enter, a, or f12.</param>
    /// <returns>The operation result.</returns>
    OperationResult keyUp(const std::string& key);

    /// <summary>
    /// Presses and releases a key.
    /// </summary>
    /// <param name="key">A portable key name such as ctrl, enter, a, or f12.</param>
    /// <returns>The operation result.</returns>
    OperationResult pressKey(const std::string& key);

    /// <summary>
    /// Presses hotkey members in order, holds them briefly, and releases in reverse order.
    /// </summary>
    /// <param name="keys">Portable key names or plus-delimited key expressions.</param>
    /// <returns>The operation result.</returns>
    OperationResult pressHotkey(const std::vector<std::string>& keys);

    /// <summary>
    /// Types UTF-8 text into the focused window using the requested input strategy.
    /// </summary>
    /// <param name="text">The UTF-8 text to type.</param>
    /// <param name="mode">Hybrid prefers physical keys and falls back to Unicode.</param>
    /// <returns>The operation result.</returns>
    OperationResult typeText(const std::string& text, TextInputMode mode = TextInputMode::Hybrid);

private:
    std::unique_ptr<DesktopAutomationPrivate> _d;
};

} // namespace desktopctl
