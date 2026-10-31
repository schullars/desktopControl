#include "private/desktopAutomationPrivate.h"

#include "platform/linux/private/linuxSessionIpc.h"

#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <dlfcn.h>
#include <linux/input-event-codes.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <sstream>
#include <thread>

namespace desktopctl {

class DesktopAutomationPrivate::XTestApi final {
public:
    using FakeKeyEvent = int (*)(Display*, unsigned int, Bool, unsigned long);
    using FakeButtonEvent = int (*)(Display*, unsigned int, Bool, unsigned long);

    XTestApi()
    {
        library_ = dlopen("libXtst.so.6", RTLD_LAZY | RTLD_LOCAL);
        if (library_ != nullptr) {
            fakeKeyEvent = reinterpret_cast<FakeKeyEvent>(dlsym(library_, "XTestFakeKeyEvent"));
            fakeButtonEvent = reinterpret_cast<FakeButtonEvent>(
                dlsym(library_, "XTestFakeButtonEvent"));
        }
    }

    ~XTestApi()
    {
        if (library_ != nullptr) {
            dlclose(library_);
        }
    }

    bool available() const
    {
        return library_ != nullptr && fakeKeyEvent != nullptr && fakeButtonEvent != nullptr;
    }

    FakeKeyEvent fakeKeyEvent = nullptr;
    FakeButtonEvent fakeButtonEvent = nullptr;

private:
    void* library_ = nullptr;
};

DesktopAutomationPrivate::DesktopAutomationPrivate() = default;

DesktopAutomationPrivate::~DesktopAutomationPrivate()
{
    if (display_ != nullptr) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
    xTest_.reset();
}

OperationResult DesktopAutomationPrivate::moveMouse(int x, int y)
{
    if (usesWaylandPortal()) {
        return LinuxSessionClient::request("POINTER_ABSOLUTE " + std::to_string(x) + " "
                                           + std::to_string(y));
    }
    OperationResult result = ensureX11();
    if (!result.success) {
        return result;
    }
    XWarpPointer(display_, None, root_, 0, 0, 0, 0, x, y);
    XFlush(display_);
    return {true, "Mouse moved."};
}

OperationResult DesktopAutomationPrivate::moveMouseRelative(int deltaX, int deltaY)
{
    if (usesWaylandPortal()) {
        return LinuxSessionClient::request("POINTER_RELATIVE " + std::to_string(deltaX) + " "
                                           + std::to_string(deltaY));
    }
    OperationResult result = ensureX11();
    if (!result.success) {
        return result;
    }
    XWarpPointer(display_, None, None, 0, 0, 0, 0, deltaX, deltaY);
    XFlush(display_);
    return {true, "Mouse moved."};
}

OperationResult DesktopAutomationPrivate::mouseDown(MouseButton button)
{
    if (usesWaylandPortal()) {
        return LinuxSessionClient::request("POINTER_BUTTON "
                                           + std::to_string(evdevButton(button)) + " 1");
    }
    OperationResult result = sendX11Button(button, true);
    if (result.success) {
        result.message = "Mouse button pressed.";
    }
    return result;
}

OperationResult DesktopAutomationPrivate::mouseUp(MouseButton button)
{
    if (usesWaylandPortal()) {
        return LinuxSessionClient::request("POINTER_BUTTON "
                                           + std::to_string(evdevButton(button)) + " 0");
    }
    OperationResult result = sendX11Button(button, false);
    if (result.success) {
        result.message = "Mouse button released.";
    }
    return result;
}

OperationResult DesktopAutomationPrivate::clickMouse(MouseButton button, int count)
{
    if (count <= 0 || count > 100) {
        return {false, "Click count must be between 1 and 100."};
    }
    for (int index = 0; index < count; ++index) {
        OperationResult result = mouseDown(button);
        if (!result.success) {
            return result;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        result = mouseUp(button);
        if (!result.success) {
            return result;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    return {true, "Mouse clicked."};
}

OperationResult DesktopAutomationPrivate::scrollMouse(int delta, WheelDirection direction)
{
    if (delta == 0) {
        return {true, "Mouse wheel did not move."};
    }
    const int notches = std::max(1, static_cast<int>(std::ceil(std::abs(delta) / 120.0)));
    if (usesWaylandPortal()) {
        const unsigned int axis = direction == WheelDirection::Vertical ? 0U : 1U;
        const int steps = delta > 0 ? -notches : notches;
        return LinuxSessionClient::request("POINTER_SCROLL " + std::to_string(axis) + " "
                                           + std::to_string(steps));
    }
    OperationResult result = ensureX11();
    if (!result.success) {
        return result;
    }
    const unsigned int button = direction == WheelDirection::Vertical
        ? (delta > 0 ? 4U : 5U)
        : (delta > 0 ? 6U : 7U);
    for (int index = 0; index < notches; ++index) {
        if (xTest_->fakeButtonEvent(display_, button, True, CurrentTime) == 0
            || xTest_->fakeButtonEvent(display_, button, False, CurrentTime) == 0) {
            return {false, "XTest failed to synthesize the mouse wheel event."};
        }
    }
    XFlush(display_);
    return {true, "Mouse wheel scrolled."};
}

OperationResult DesktopAutomationPrivate::keyDown(const std::string& key)
{
    const std::optional<KeySym> keysym = keysymForName(key);
    if (!keysym) {
        return {false, "Unknown key: " + key};
    }
    OperationResult result = usesWaylandPortal() ? sendPortalKeysym(*keysym, true)
                                                 : sendX11Keysym(*keysym, true);
    if (result.success) {
        result.message = "Key pressed.";
    }
    return result;
}

OperationResult DesktopAutomationPrivate::keyUp(const std::string& key)
{
    const std::optional<KeySym> keysym = keysymForName(key);
    if (!keysym) {
        return {false, "Unknown key: " + key};
    }
    OperationResult result = usesWaylandPortal() ? sendPortalKeysym(*keysym, false)
                                                 : sendX11Keysym(*keysym, false);
    if (result.success) {
        result.message = "Key released.";
    }
    return result;
}

OperationResult DesktopAutomationPrivate::pressKey(const std::string& key)
{
    OperationResult result = keyDown(key);
    if (!result.success) {
        return result;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    result = keyUp(key);
    if (!result.success) {
        return result;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    return {true, "Key pressed and released."};
}

OperationResult DesktopAutomationPrivate::pressHotkey(const std::vector<std::string>& keys)
{
    const std::vector<std::string> expanded = expandHotkey(keys);
    if (expanded.empty()) {
        return {false, "A hotkey requires at least one key."};
    }
    std::vector<KeySym> keysyms;
    keysyms.reserve(expanded.size());
    for (const std::string& key : expanded) {
        const std::optional<KeySym> keysym = keysymForName(key);
        if (!keysym) {
            return {false, "Unknown key: " + key};
        }
        keysyms.push_back(*keysym);
    }

    std::size_t pressed = 0;
    for (KeySym keysym : keysyms) {
        OperationResult result = usesWaylandPortal() ? sendPortalKeysym(keysym, true)
                                                     : sendX11Keysym(keysym, true);
        if (!result.success) {
            for (std::size_t index = pressed; index > 0; --index) {
                if (usesWaylandPortal()) {
                    sendPortalKeysym(keysyms[index - 1U], false);
                } else {
                    sendX11Keysym(keysyms[index - 1U], false);
                }
            }
            return result;
        }
        ++pressed;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    for (auto iterator = keysyms.rbegin(); iterator != keysyms.rend(); ++iterator) {
        OperationResult result = usesWaylandPortal() ? sendPortalKeysym(*iterator, false)
                                                     : sendX11Keysym(*iterator, false);
        if (!result.success) {
            return result;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return {true, "Hotkey pressed."};
}

OperationResult DesktopAutomationPrivate::typeText(const std::string& text, TextInputMode mode)
{
    if (text.empty()) {
        return {true, "No text to type."};
    }
    std::vector<std::uint32_t> codepoints;
    std::string error;
    if (!decodeUtf8(text, codepoints, error)) {
        return {false, error};
    }
    for (std::uint32_t codepoint : codepoints) {
        OperationResult result = usesWaylandPortal() ? typePortalCodepoint(codepoint, mode)
                                                     : typeX11Codepoint(codepoint, mode);
        if (!result.success) {
            return result;
        }
    }
    return {true, "Text typed."};
}

std::optional<KeySym> DesktopAutomationPrivate::keysymForName(const std::string& key)
{
    std::string normalized = key;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    struct NamedKey {
        const char* name;
        KeySym keysym;
    };
    static constexpr NamedKey NamedKeys[] = {
        {"ctrl", XK_Control_L}, {"control", XK_Control_L}, {"shift", XK_Shift_L},
        {"alt", XK_Alt_L}, {"win", XK_Super_L}, {"super", XK_Super_L},
        {"enter", XK_Return}, {"return", XK_Return}, {"tab", XK_Tab},
        {"space", XK_space}, {"escape", XK_Escape}, {"esc", XK_Escape},
        {"backspace", XK_BackSpace}, {"delete", XK_Delete}, {"insert", XK_Insert},
        {"home", XK_Home}, {"end", XK_End}, {"pageup", XK_Page_Up},
        {"pagedown", XK_Page_Down}, {"left", XK_Left}, {"right", XK_Right},
        {"up", XK_Up}, {"down", XK_Down}, {"printscreen", XK_Print},
        {"capslock", XK_Caps_Lock}
    };
    for (const NamedKey& named : NamedKeys) {
        if (normalized == named.name) {
            return named.keysym;
        }
    }
    if (normalized.size() == 1U
        && ((normalized[0] >= 'a' && normalized[0] <= 'z')
            || (normalized[0] >= '0' && normalized[0] <= '9'))) {
        return static_cast<KeySym>(normalized[0]);
    }
    if (normalized.size() >= 2U && normalized[0] == 'f') {
        try {
            const int functionNumber = std::stoi(normalized.substr(1));
            if (functionNumber >= 1 && functionNumber <= 24) {
                return static_cast<KeySym>(XK_F1 + functionNumber - 1);
            }
        } catch (...) {
        }
    }
    if (normalized.rfind("numpad", 0) == 0 && normalized.size() == 7U
        && normalized.back() >= '0' && normalized.back() <= '9') {
        return static_cast<KeySym>(XK_KP_0 + normalized.back() - '0');
    }
    return std::nullopt;
}

std::vector<std::string> DesktopAutomationPrivate::expandHotkey(
    const std::vector<std::string>& keys)
{
    std::vector<std::string> expanded;
    for (const std::string& expression : keys) {
        std::size_t begin = 0;
        while (begin <= expression.size()) {
            const std::size_t separator = expression.find('+', begin);
            const std::string part = expression.substr(
                begin, separator == std::string::npos ? std::string::npos : separator - begin);
            if (part.empty()) {
                return {};
            }
            expanded.push_back(part);
            if (separator == std::string::npos) {
                break;
            }
            begin = separator + 1U;
        }
    }
    return expanded;
}

bool DesktopAutomationPrivate::decodeUtf8(const std::string& text,
                                           std::vector<std::uint32_t>& codepoints,
                                           std::string& error)
{
    codepoints.clear();
    for (std::size_t index = 0; index < text.size();) {
        const auto first = static_cast<unsigned char>(text[index]);
        std::uint32_t value = 0;
        std::size_t length = 0;
        if (first < 0x80U) {
            value = first;
            length = 1;
        } else if ((first & 0xe0U) == 0xc0U) {
            value = first & 0x1fU;
            length = 2;
        } else if ((first & 0xf0U) == 0xe0U) {
            value = first & 0x0fU;
            length = 3;
        } else if ((first & 0xf8U) == 0xf0U) {
            value = first & 0x07U;
            length = 4;
        } else {
            error = "Text contains invalid UTF-8.";
            return false;
        }
        if (index + length > text.size()) {
            error = "Text contains truncated UTF-8.";
            return false;
        }
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xc0U) != 0x80U) {
                error = "Text contains invalid UTF-8 continuation bytes.";
                return false;
            }
            value = (value << 6U) | (continuation & 0x3fU);
        }
        const bool overlong = (length == 2U && value < 0x80U)
            || (length == 3U && value < 0x800U) || (length == 4U && value < 0x10000U);
        if (overlong || value > 0x10ffffU || (value >= 0xd800U && value <= 0xdfffU)) {
            error = "Text contains an invalid Unicode code point.";
            return false;
        }
        codepoints.push_back(value);
        index += length;
    }
    return true;
}

KeySym DesktopAutomationPrivate::keysymForCodepoint(std::uint32_t codepoint)
{
    if (codepoint == '\n' || codepoint == '\r') {
        return XK_Return;
    }
    if (codepoint == '\t') {
        return XK_Tab;
    }
    if (codepoint <= 0xffU) {
        return static_cast<KeySym>(codepoint);
    }
    return static_cast<KeySym>(0x01000000U | codepoint);
}

int DesktopAutomationPrivate::evdevButton(MouseButton button)
{
    switch (button) {
    case MouseButton::Left:
        return BTN_LEFT;
    case MouseButton::Right:
        return BTN_RIGHT;
    case MouseButton::Middle:
        return BTN_MIDDLE;
    }
    return BTN_LEFT;
}

bool DesktopAutomationPrivate::usesWaylandPortal() const
{
    return LinuxSessionClient::isWaylandSession();
}

OperationResult DesktopAutomationPrivate::ensureX11()
{
    if (display_ == nullptr) {
        display_ = XOpenDisplay(nullptr);
        if (display_ == nullptr) {
            return {false, "Unable to open the X11 display."};
        }
        root_ = DefaultRootWindow(display_);
        xTest_ = std::make_unique<XTestApi>();
    }
    if (xTest_ == nullptr || !xTest_->available()) {
        return {false, "The XTest runtime library is required for Linux input control."};
    }
    return {true, {}};
}

OperationResult DesktopAutomationPrivate::sendX11Button(MouseButton button, bool pressed)
{
    OperationResult result = ensureX11();
    if (!result.success) {
        return result;
    }
    unsigned int number = 1U;
    if (button == MouseButton::Middle) {
        number = 2U;
    } else if (button == MouseButton::Right) {
        number = 3U;
    }
    if (xTest_->fakeButtonEvent(display_, number, pressed ? True : False, CurrentTime) == 0) {
        return {false, "XTest failed to synthesize the mouse button event."};
    }
    XFlush(display_);
    return {true, {}};
}

OperationResult DesktopAutomationPrivate::sendX11Keysym(KeySym keysym, bool pressed)
{
    OperationResult result = ensureX11();
    if (!result.success) {
        return result;
    }
    const KeyCode keycode = XKeysymToKeycode(display_, keysym);
    if (keycode == 0) {
        return {false, "The active X11 keyboard map cannot represent keysym "
                           + std::to_string(static_cast<unsigned long>(keysym)) + "."};
    }
    if (xTest_->fakeKeyEvent(display_, keycode, pressed ? True : False, CurrentTime) == 0) {
        return {false, "XTest failed to synthesize the keyboard event."};
    }
    XFlush(display_);
    return {true, {}};
}

OperationResult DesktopAutomationPrivate::sendPortalKeysym(KeySym keysym, bool pressed)
{
    return LinuxSessionClient::request("KEYSYM "
                                       + std::to_string(static_cast<unsigned long>(keysym)) + " "
                                       + (pressed ? "1" : "0"));
}

OperationResult DesktopAutomationPrivate::typeX11Codepoint(std::uint32_t codepoint,
                                                            TextInputMode mode)
{
    OperationResult result = ensureX11();
    if (!result.success) {
        return result;
    }
    const KeySym keysym = keysymForCodepoint(codepoint);
    KeyCode keycode = 0;
    bool shift = false;
    bool level3 = false;
    if (mode != TextInputMode::Unicode
        && findX11Character(keysym, keycode, shift, level3)) {
        const KeyCode shiftCode = XKeysymToKeycode(display_, XK_Shift_L);
        const KeyCode level3Code = XKeysymToKeycode(display_, XK_ISO_Level3_Shift);
        if (level3 && level3Code == 0) {
            return {false, "The X11 keyboard map has no ISO Level3 Shift key."};
        }
        if (shift && xTest_->fakeKeyEvent(display_, shiftCode, True, CurrentTime) == 0) {
            return {false, "XTest failed to press Shift while typing text."};
        }
        if (level3 && xTest_->fakeKeyEvent(display_, level3Code, True, CurrentTime) == 0) {
            return {false, "XTest failed to press ISO Level3 Shift while typing text."};
        }
        const bool keyDownSucceeded = xTest_->fakeKeyEvent(display_, keycode, True, CurrentTime)
            != 0;
        const bool keyUpSucceeded = xTest_->fakeKeyEvent(display_, keycode, False, CurrentTime)
            != 0;
        if (level3) {
            xTest_->fakeKeyEvent(display_, level3Code, False, CurrentTime);
        }
        if (shift) {
            xTest_->fakeKeyEvent(display_, shiftCode, False, CurrentTime);
        }
        XFlush(display_);
        if (!keyDownSucceeded || !keyUpSucceeded) {
            return {false, "XTest failed while typing text."};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
        return {true, {}};
    }
    if (mode == TextInputMode::Physical) {
        return {false, "Physical text input cannot represent Unicode code point "
                           + std::to_string(codepoint) + " in the active X11 keyboard map."};
    }
    return sendX11UnicodeCompose(codepoint);
}

OperationResult DesktopAutomationPrivate::typePortalCodepoint(std::uint32_t codepoint,
                                                               TextInputMode mode)
{
    if (mode == TextInputMode::Physical) {
        return {false,
                "Wayland portals expose keysyms rather than the target layout's physical scan "
                "codes; use type or type-unicode, or log in to Ubuntu on Xorg for physical text."};
    }
    const KeySym keysym = keysymForCodepoint(codepoint);
    OperationResult result = sendPortalKeysym(keysym, true);
    if (!result.success) {
        return result;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    result = sendPortalKeysym(keysym, false);
    if (result.success) {
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
    return result;
}

bool DesktopAutomationPrivate::findX11Character(KeySym keysym, KeyCode& keycode,
                                                 bool& shift, bool& level3) const
{
    int minimumKeycode = 0;
    int maximumKeycode = 0;
    XDisplayKeycodes(display_, &minimumKeycode, &maximumKeycode);
    for (int candidate = minimumKeycode; candidate <= maximumKeycode; ++candidate) {
        for (int level = 0; level < 4; ++level) {
            if (XkbKeycodeToKeysym(display_, static_cast<KeyCode>(candidate), 0, level)
                == keysym) {
                keycode = static_cast<KeyCode>(candidate);
                shift = level == 1 || level == 3;
                level3 = level >= 2;
                return true;
            }
        }
    }
    return false;
}

OperationResult DesktopAutomationPrivate::sendX11UnicodeCompose(std::uint32_t codepoint)
{
    OperationResult result = sendX11Keysym(XK_Control_L, true);
    if (!result.success) {
        return result;
    }
    result = sendX11Keysym(XK_Shift_L, true);
    if (!result.success) {
        sendX11Keysym(XK_Control_L, false);
        return result;
    }
    result = sendX11Keysym(XK_u, true);
    if (result.success) {
        result = sendX11Keysym(XK_u, false);
    }
    sendX11Keysym(XK_Shift_L, false);
    sendX11Keysym(XK_Control_L, false);
    if (!result.success) {
        return result;
    }

    std::ostringstream hexadecimal;
    hexadecimal << std::hex << codepoint;
    for (char digit : hexadecimal.str()) {
        const KeySym digitKeysym = static_cast<KeySym>(digit);
        result = sendX11Keysym(digitKeysym, true);
        if (result.success) {
            result = sendX11Keysym(digitKeysym, false);
        }
        if (!result.success) {
            return result;
        }
    }
    result = sendX11Keysym(XK_Return, true);
    if (result.success) {
        result = sendX11Keysym(XK_Return, false);
    }
    return result;
}

} // namespace desktopctl
