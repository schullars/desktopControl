#include "private/desktopAutomationPrivate.h"

#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace desktopctl {

DesktopAutomationPrivate::DesktopAutomationPrivate()
{
    // The command contract uses physical virtual-desktop pixels, so prevent DPI virtualization.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

DesktopAutomationPrivate::~DesktopAutomationPrivate() = default;

OperationResult DesktopAutomationPrivate::windowsFailure(const std::string& operation)
{
    return {false,
            operation + " failed with Win32 error "
                + std::to_string(static_cast<unsigned long>(GetLastError())) + "."};
}

OperationResult DesktopAutomationPrivate::moveMouse(int x, int y)
{
    if (SetCursorPos(x, y) == FALSE) {
        return windowsFailure("SetCursorPos");
    }
    return {true, "Mouse moved."};
}

OperationResult DesktopAutomationPrivate::moveMouseRelative(int deltaX, int deltaY)
{
    POINT current{};
    if (GetCursorPos(&current) == FALSE) {
        return windowsFailure("GetCursorPos");
    }
    const std::int64_t targetX = static_cast<std::int64_t>(current.x) + deltaX;
    const std::int64_t targetY = static_cast<std::int64_t>(current.y) + deltaY;
    if (targetX < std::numeric_limits<int>::min()
        || targetX > std::numeric_limits<int>::max()
        || targetY < std::numeric_limits<int>::min()
        || targetY > std::numeric_limits<int>::max()) {
        return {false, "Relative mouse movement exceeds the coordinate range."};
    }
    if (SetCursorPos(static_cast<int>(targetX), static_cast<int>(targetY)) == FALSE) {
        return windowsFailure("SetCursorPos");
    }
    return {true, "Mouse moved."};
}

OperationResult DesktopAutomationPrivate::sendMouseButton(MouseButton button, bool pressed)
{
    DWORD flag = 0;
    switch (button) {
    case MouseButton::Left:
        flag = pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        break;
    case MouseButton::Right:
        flag = pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
        break;
    case MouseButton::Middle:
        flag = pressed ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
        break;
    }

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flag;
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        return windowsFailure(pressed ? "SendInput(mouse down)" : "SendInput(mouse up)");
    }
    return {true, {}};
}

OperationResult DesktopAutomationPrivate::mouseDown(MouseButton button)
{
    OperationResult result = sendMouseButton(button, true);
    if (result.success) {
        result.message = "Mouse button pressed.";
    }
    return result;
}

OperationResult DesktopAutomationPrivate::mouseUp(MouseButton button)
{
    OperationResult result = sendMouseButton(button, false);
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

    std::vector<INPUT> inputs(static_cast<std::size_t>(count) * 2U);
    DWORD downFlag = 0;
    DWORD upFlag = 0;
    switch (button) {
    case MouseButton::Left:
        downFlag = MOUSEEVENTF_LEFTDOWN;
        upFlag = MOUSEEVENTF_LEFTUP;
        break;
    case MouseButton::Right:
        downFlag = MOUSEEVENTF_RIGHTDOWN;
        upFlag = MOUSEEVENTF_RIGHTUP;
        break;
    case MouseButton::Middle:
        downFlag = MOUSEEVENTF_MIDDLEDOWN;
        upFlag = MOUSEEVENTF_MIDDLEUP;
        break;
    }

    for (int index = 0; index < count; ++index) {
        INPUT& down = inputs[static_cast<std::size_t>(index) * 2U];
        down.type = INPUT_MOUSE;
        down.mi.dwFlags = downFlag;
        INPUT& up = inputs[static_cast<std::size_t>(index) * 2U + 1U];
        up.type = INPUT_MOUSE;
        up.mi.dwFlags = upFlag;
    }

    const UINT expected = static_cast<UINT>(inputs.size());
    if (SendInput(expected, inputs.data(), sizeof(INPUT)) != expected) {
        return windowsFailure("SendInput(mouse click)");
    }
    return {true, "Mouse clicked."};
}

OperationResult DesktopAutomationPrivate::scrollMouse(int delta, WheelDirection direction)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.mouseData = static_cast<DWORD>(delta);
    input.mi.dwFlags = direction == WheelDirection::Vertical ? MOUSEEVENTF_WHEEL
                                                            : MOUSEEVENTF_HWHEEL;
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        return windowsFailure("SendInput(mouse wheel)");
    }
    return {true, "Mouse wheel scrolled."};
}

std::string DesktopAutomationPrivate::normalizeKey(const std::string& key)
{
    std::string result;
    result.reserve(key.size());
    for (const unsigned char character : key) {
        if (character != '-' && character != '_' && character != ' ') {
            result.push_back(static_cast<char>(std::tolower(character)));
        }
    }
    return result;
}

std::optional<DesktopAutomationPrivate::KeyDefinition>
DesktopAutomationPrivate::parseKey(const std::string& key)
{
    if (key.size() == 1) {
        const unsigned char character = static_cast<unsigned char>(key.front());
        if (std::isalpha(character) != 0) {
            return KeyDefinition{static_cast<std::uint16_t>(std::toupper(character)), false};
        }
        if (std::isdigit(character) != 0) {
            return KeyDefinition{static_cast<std::uint16_t>(character), false};
        }

        static const std::unordered_map<char, int> punctuation{
            {';', VK_OEM_1},       {'=', VK_OEM_PLUS},   {',', VK_OEM_COMMA},
            {'-', VK_OEM_MINUS},   {'.', VK_OEM_PERIOD}, {'/', VK_OEM_2},
            {'`', VK_OEM_3},       {'[', VK_OEM_4},      {'\\', VK_OEM_5},
            {']', VK_OEM_6},       {'\'', VK_OEM_7}};
        const auto punctuationIt = punctuation.find(static_cast<char>(character));
        if (punctuationIt != punctuation.end()) {
            return KeyDefinition{static_cast<std::uint16_t>(punctuationIt->second), false};
        }
    }

    const std::string normalized = normalizeKey(key);
    if (normalized.size() >= 2 && normalized.front() == 'f') {
        int functionNumber = 0;
        const char* begin = normalized.data() + 1;
        const char* end = normalized.data() + normalized.size();
        const auto conversion = std::from_chars(begin, end, functionNumber);
        if (conversion.ec == std::errc() && conversion.ptr == end && functionNumber >= 1
            && functionNumber <= 24) {
            return KeyDefinition{static_cast<std::uint16_t>(VK_F1 + functionNumber - 1), false};
        }
    }

    static const std::unordered_map<std::string, KeyDefinition> names{
        {"backspace", {VK_BACK, false}},       {"tab", {VK_TAB, false}},
        {"enter", {VK_RETURN, false}},         {"return", {VK_RETURN, false}},
        {"shift", {VK_SHIFT, false}},          {"leftshift", {VK_LSHIFT, false}},
        {"rightshift", {VK_RSHIFT, false}},    {"ctrl", {VK_CONTROL, false}},
        {"control", {VK_CONTROL, false}},      {"leftctrl", {VK_LCONTROL, false}},
        {"rightctrl", {VK_RCONTROL, true}},    {"alt", {VK_MENU, false}},
        {"leftalt", {VK_LMENU, false}},        {"rightalt", {VK_RMENU, true}},
        {"pause", {VK_PAUSE, false}},          {"capslock", {VK_CAPITAL, false}},
        {"escape", {VK_ESCAPE, false}},        {"esc", {VK_ESCAPE, false}},
        {"space", {VK_SPACE, false}},          {"pageup", {VK_PRIOR, true}},
        {"pagedown", {VK_NEXT, true}},         {"end", {VK_END, true}},
        {"home", {VK_HOME, true}},             {"left", {VK_LEFT, true}},
        {"up", {VK_UP, true}},                 {"right", {VK_RIGHT, true}},
        {"down", {VK_DOWN, true}},             {"printscreen", {VK_SNAPSHOT, true}},
        {"insert", {VK_INSERT, true}},         {"delete", {VK_DELETE, true}},
        {"del", {VK_DELETE, true}},            {"leftwin", {VK_LWIN, true}},
        {"win", {VK_LWIN, true}},              {"rightwin", {VK_RWIN, true}},
        {"apps", {VK_APPS, true}},             {"numlock", {VK_NUMLOCK, true}},
        {"scrolllock", {VK_SCROLL, false}},    {"numpad0", {VK_NUMPAD0, false}},
        {"numpad1", {VK_NUMPAD1, false}},      {"numpad2", {VK_NUMPAD2, false}},
        {"numpad3", {VK_NUMPAD3, false}},      {"numpad4", {VK_NUMPAD4, false}},
        {"numpad5", {VK_NUMPAD5, false}},      {"numpad6", {VK_NUMPAD6, false}},
        {"numpad7", {VK_NUMPAD7, false}},      {"numpad8", {VK_NUMPAD8, false}},
        {"numpad9", {VK_NUMPAD9, false}},      {"multiply", {VK_MULTIPLY, false}},
        {"add", {VK_ADD, false}},              {"subtract", {VK_SUBTRACT, false}},
        {"decimal", {VK_DECIMAL, false}},      {"divide", {VK_DIVIDE, true}},
        {"semicolon", {VK_OEM_1, false}},      {"equal", {VK_OEM_PLUS, false}},
        {"comma", {VK_OEM_COMMA, false}},      {"minus", {VK_OEM_MINUS, false}},
        {"period", {VK_OEM_PERIOD, false}},    {"slash", {VK_OEM_2, false}},
        {"backtick", {VK_OEM_3, false}},       {"leftbracket", {VK_OEM_4, false}},
        {"backslash", {VK_OEM_5, false}},      {"rightbracket", {VK_OEM_6, false}},
        {"quote", {VK_OEM_7, false}}};
    const auto nameIt = names.find(normalized);
    if (nameIt == names.end()) {
        return std::nullopt;
    }
    return nameIt->second;
}

OperationResult DesktopAutomationPrivate::sendKey(const KeyDefinition& key, bool pressed)
{
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key.virtualKey;
    input.ki.dwFlags = (key.extended ? KEYEVENTF_EXTENDEDKEY : 0U)
        | (pressed ? 0U : KEYEVENTF_KEYUP);
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        return windowsFailure(pressed ? "SendInput(key down)" : "SendInput(key up)");
    }
    return {true, {}};
}

OperationResult DesktopAutomationPrivate::keyDown(const std::string& key)
{
    const std::optional<KeyDefinition> definition = parseKey(key);
    if (!definition) {
        return {false, "Unknown key: " + key};
    }
    OperationResult result = sendKey(*definition, true);
    if (result.success) {
        result.message = "Key pressed.";
    }
    return result;
}

OperationResult DesktopAutomationPrivate::keyUp(const std::string& key)
{
    const std::optional<KeyDefinition> definition = parseKey(key);
    if (!definition) {
        return {false, "Unknown key: " + key};
    }
    OperationResult result = sendKey(*definition, false);
    if (result.success) {
        result.message = "Key released.";
    }
    return result;
}

OperationResult DesktopAutomationPrivate::pressKey(const std::string& key)
{
    const std::optional<KeyDefinition> definition = parseKey(key);
    if (!definition) {
        return {false, "Unknown key: " + key};
    }

    INPUT inputs[2]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = definition->virtualKey;
    inputs[0].ki.dwFlags = definition->extended ? KEYEVENTF_EXTENDEDKEY : 0U;
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;
    if (SendInput(2, inputs, sizeof(INPUT)) != 2) {
        return windowsFailure("SendInput(key press)");
    }
    return {true, "Key pressed and released."};
}

OperationResult DesktopAutomationPrivate::pressHotkey(const std::vector<std::string>& keys)
{
    if (keys.empty()) {
        return {false, "A hotkey requires at least one key."};
    }

    std::vector<KeyDefinition> definitions;
    definitions.reserve(keys.size());
    for (const std::string& key : keys) {
        const std::optional<KeyDefinition> definition = parseKey(key);
        if (!definition) {
            return {false, "Unknown key: " + key};
        }
        definitions.push_back(*definition);
    }
    if (definitions.size() > std::numeric_limits<UINT>::max() / 2U) {
        return {false, "The hotkey contains too many keys."};
    }

    std::vector<INPUT> inputs(definitions.size() * 2U);
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        INPUT& input = inputs[index];
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = definitions[index].virtualKey;
        input.ki.dwFlags = definitions[index].extended ? KEYEVENTF_EXTENDEDKEY : 0U;
    }
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const KeyDefinition& definition = definitions[definitions.size() - index - 1U];
        INPUT& input = inputs[definitions.size() + index];
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = definition.virtualKey;
        input.ki.dwFlags = (definition.extended ? KEYEVENTF_EXTENDEDKEY : 0U)
            | KEYEVENTF_KEYUP;
    }

    const UINT expected = static_cast<UINT>(inputs.size());
    if (SendInput(expected, inputs.data(), sizeof(INPUT)) != expected) {
        return windowsFailure("SendInput(hotkey)");
    }
    return {true, "Hotkey pressed."};
}

OperationResult DesktopAutomationPrivate::typeText(const std::string& text)
{
    if (text.empty()) {
        return {true, "No text to type."};
    }
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {false, "Text is too large."};
    }

    const int utf16Length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                                 static_cast<int>(text.size()), nullptr, 0);
    if (utf16Length <= 0) {
        return windowsFailure("UTF-8 conversion");
    }
    std::wstring utf16(static_cast<std::size_t>(utf16Length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), utf16.data(), utf16Length)
        != utf16Length) {
        return windowsFailure("UTF-8 conversion");
    }
    if (utf16.size() > std::numeric_limits<UINT>::max() / 2U) {
        return {false, "Text is too large."};
    }

    std::vector<INPUT> inputs(utf16.size() * 2U);
    for (std::size_t index = 0; index < utf16.size(); ++index) {
        INPUT& down = inputs[index * 2U];
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = static_cast<WORD>(utf16[index]);
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        INPUT& up = inputs[index * 2U + 1U];
        up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    }

    const UINT expected = static_cast<UINT>(inputs.size());
    if (SendInput(expected, inputs.data(), sizeof(INPUT)) != expected) {
        return windowsFailure("SendInput(Unicode text)");
    }
    return {true, "Text typed."};
}

} // namespace desktopctl
