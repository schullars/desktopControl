#include "platform/windows/private/keyboardInputWindows.h"

#include <charconv>
#include <cctype>
#include <limits>
#include <unordered_map>

namespace desktopctl {

std::string KeyboardInputWindows::normalizeKey(const std::string& key)
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

std::optional<KeyboardInputWindows::KeyDefinition>
KeyboardInputWindows::parseKey(const std::string& key)
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
            {';', VK_OEM_1},       {'=', VK_OEM_PLUS},   {'+', VK_OEM_PLUS},
            {',', VK_OEM_COMMA},   {'-', VK_OEM_MINUS},  {'.', VK_OEM_PERIOD},
            {'/', VK_OEM_2},       {'`', VK_OEM_3},      {'[', VK_OEM_4},
            {'\\', VK_OEM_5},      {']', VK_OEM_6},      {'\'', VK_OEM_7}};
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
        {"plus", {VK_OEM_PLUS, false}},        {"comma", {VK_OEM_COMMA, false}},
        {"minus", {VK_OEM_MINUS, false}},      {"period", {VK_OEM_PERIOD, false}},
        {"slash", {VK_OEM_2, false}},          {"backtick", {VK_OEM_3, false}},
        {"leftbracket", {VK_OEM_4, false}},    {"backslash", {VK_OEM_5, false}},
        {"rightbracket", {VK_OEM_6, false}},   {"quote", {VK_OEM_7, false}}};
    const auto nameIt = names.find(normalized);
    if (nameIt == names.end()) {
        return std::nullopt;
    }
    return nameIt->second;
}

HKL KeyboardInputWindows::foregroundKeyboardLayout()
{
    const HWND foregroundWindow = GetForegroundWindow();
    DWORD threadId = 0;
    if (foregroundWindow != nullptr) {
        threadId = GetWindowThreadProcessId(foregroundWindow, nullptr);
    }
    return GetKeyboardLayout(threadId);
}

INPUT KeyboardInputWindows::createInput(const KeyDefinition& key, bool pressed, HKL layout)
{
    INPUT input{};
    input.type = INPUT_KEYBOARD;

    const UINT mappedScanCode = key.virtualKey == VK_PAUSE
        ? 0U
        : MapVirtualKeyExW(key.virtualKey, MAPVK_VK_TO_VSC_EX, layout);
    if (mappedScanCode != 0U) {
        input.ki.wScan = static_cast<WORD>(mappedScanCode & 0xFFU);
        input.ki.dwFlags = KEYEVENTF_SCANCODE;
    } else {
        input.ki.wVk = key.virtualKey;
    }

    if (key.extended) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    if (!pressed) {
        input.ki.dwFlags |= KEYEVENTF_KEYUP;
    }
    return input;
}

bool KeyboardInputWindows::expandHotkeyKeys(const std::vector<std::string>& keys,
                                             std::vector<std::string>& expandedKeys,
                                             std::string& error)
{
    if (keys.empty()) {
        error = "A hotkey requires at least one key.";
        return false;
    }

    for (const std::string& keyExpression : keys) {
        std::size_t segmentStart = 0;
        while (segmentStart <= keyExpression.size()) {
            const std::size_t separator = keyExpression.find('+', segmentStart);
            const std::size_t segmentEnd = separator == std::string::npos
                ? keyExpression.size()
                : separator;
            if (segmentEnd == segmentStart) {
                error = "Hotkey expressions must not contain empty key names: " + keyExpression;
                return false;
            }
            expandedKeys.push_back(keyExpression.substr(segmentStart, segmentEnd - segmentStart));
            if (separator == std::string::npos) {
                break;
            }
            segmentStart = separator + 1U;
        }
    }
    return true;
}

bool KeyboardInputWindows::buildHotkeyInputs(const std::vector<std::string>& keys,
                                              HKL layout,
                                              std::vector<INPUT>& downInputs,
                                              std::vector<INPUT>& upInputs,
                                              std::string& error)
{
    downInputs.clear();
    upInputs.clear();
    error.clear();

    std::vector<std::string> expandedKeys;
    if (!expandHotkeyKeys(keys, expandedKeys, error)) {
        return false;
    }
    if (expandedKeys.size() > std::numeric_limits<UINT>::max()) {
        error = "The hotkey contains too many keys.";
        return false;
    }

    std::vector<KeyDefinition> definitions;
    definitions.reserve(expandedKeys.size());
    for (const std::string& key : expandedKeys) {
        const std::optional<KeyDefinition> definition = parseKey(key);
        if (!definition) {
            error = "Unknown key: " + key;
            return false;
        }
        definitions.push_back(*definition);
    }

    downInputs.reserve(definitions.size());
    upInputs.reserve(definitions.size());
    for (const KeyDefinition& definition : definitions) {
        downInputs.push_back(createInput(definition, true, layout));
    }
    for (auto definition = definitions.rbegin(); definition != definitions.rend(); ++definition) {
        upInputs.push_back(createInput(*definition, false, layout));
    }
    return true;
}

} // namespace desktopctl
