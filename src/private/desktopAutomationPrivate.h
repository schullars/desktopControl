#pragma once

#include "desktopAutomation.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#if defined(__linux__)
#include <X11/Xlib.h>
#endif

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
    OperationResult typeText(const std::string& text, TextInputMode mode);

private:
#if defined(_WIN32)
    static OperationResult windowsFailure(const std::string& operation);
    static OperationResult sendMouseButton(MouseButton button, bool pressed);
#elif defined(__linux__)
    class XTestApi;

    static std::optional<KeySym> keysymForName(const std::string& key);
    static std::vector<std::string> expandHotkey(const std::vector<std::string>& keys);
    static bool decodeUtf8(const std::string& text, std::vector<std::uint32_t>& codepoints,
                           std::string& error);
    static KeySym keysymForCodepoint(std::uint32_t codepoint);
    static int evdevButton(MouseButton button);
    bool usesWaylandPortal() const;
    OperationResult ensureX11();
    OperationResult sendX11Button(MouseButton button, bool pressed);
    OperationResult sendX11Keysym(KeySym keysym, bool pressed);
    OperationResult sendPortalKeysym(KeySym keysym, bool pressed);
    OperationResult typeX11Codepoint(std::uint32_t codepoint, TextInputMode mode);
    OperationResult typePortalCodepoint(std::uint32_t codepoint, TextInputMode mode);
    bool findX11Character(KeySym keysym, KeyCode& keycode, bool& shift, bool& level3) const;
    OperationResult sendX11UnicodeCompose(std::uint32_t codepoint);

    Display* display_ = nullptr;
    Window root_ = None;
    std::unique_ptr<XTestApi> xTest_;
#endif
};

} // namespace desktopctl
