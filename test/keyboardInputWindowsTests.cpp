#include "platform/windows/private/keyboardInputWindows.h"

#include <Windows.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    using desktopctl::KeyboardInputWindows;

    int failures = 0;
    const HKL layout = GetKeyboardLayout(0);
    const auto require = [&failures](bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << message << '\n';
            ++failures;
        }
    };
    const auto verifySequence = [&require, layout](
                                    const std::vector<std::string>& expressions,
                                    const std::vector<KeyboardInputWindows::KeyDefinition>& keys,
                                    const std::string& name) {
        std::vector<INPUT> downInputs;
        std::vector<INPUT> upInputs;
        std::string error;
        require(KeyboardInputWindows::buildHotkeyInputs(
                    expressions, layout, downInputs, upInputs, error),
                name + " did not parse: " + error);
        require(downInputs.size() == keys.size(), name + " down count is incorrect.");
        require(upInputs.size() == keys.size(), name + " up count is incorrect.");
        if (downInputs.size() != keys.size() || upInputs.size() != keys.size()) {
            return;
        }

        for (std::size_t index = 0; index < keys.size(); ++index) {
            const INPUT expectedDown = KeyboardInputWindows::createInput(keys[index], true, layout);
            const INPUT expectedUp = KeyboardInputWindows::createInput(
                keys[keys.size() - index - 1U], false, layout);
            require(downInputs[index].type == expectedDown.type
                        && downInputs[index].ki.wVk == expectedDown.ki.wVk
                        && downInputs[index].ki.wScan == expectedDown.ki.wScan
                        && downInputs[index].ki.dwFlags == expectedDown.ki.dwFlags,
                    name + " contains an incorrect key-down input.");
            require(upInputs[index].type == expectedUp.type
                        && upInputs[index].ki.wVk == expectedUp.ki.wVk
                        && upInputs[index].ki.wScan == expectedUp.ki.wScan
                        && upInputs[index].ki.dwFlags == expectedUp.ki.dwFlags,
                    name + " contains an incorrect key-up input or release order.");
        }
    };

    verifySequence({"shift", "2"}, {{VK_SHIFT, false}, {'2', false}}, "Shift+2");
    verifySequence({"shift", "1"}, {{VK_SHIFT, false}, {'1', false}}, "Shift+1");
    verifySequence({"shift", "minus"},
                   {{VK_SHIFT, false}, {VK_OEM_MINUS, false}},
                   "Shift+minus");
    verifySequence({"ctrl", "a"}, {{VK_CONTROL, false}, {'A', false}}, "Ctrl+A");
    verifySequence({"ctrl", "c"}, {{VK_CONTROL, false}, {'C', false}}, "Ctrl+C");
    verifySequence({"ctrl", "v"}, {{VK_CONTROL, false}, {'V', false}}, "Ctrl+V");
    verifySequence({"ctrl", "z"}, {{VK_CONTROL, false}, {'Z', false}}, "Ctrl+Z");
    verifySequence({"ctrl", "s"}, {{VK_CONTROL, false}, {'S', false}}, "Ctrl+S");
    verifySequence({"ctrl", "l"}, {{VK_CONTROL, false}, {'L', false}}, "Ctrl+L");
    verifySequence({"ctrl", "n"}, {{VK_CONTROL, false}, {'N', false}}, "Ctrl+N");
    verifySequence({"ctrl", "shift", "escape"},
                   {{VK_CONTROL, false}, {VK_SHIFT, false}, {VK_ESCAPE, false}},
                   "Ctrl+Shift+Escape");
    verifySequence({"ctrl+shift+escape"},
                   {{VK_CONTROL, false}, {VK_SHIFT, false}, {VK_ESCAPE, false}},
                   "Ctrl+Shift+Escape expression");
    verifySequence({"alt", "f4"}, {{VK_MENU, false}, {VK_F4, false}}, "Alt+F4");
    verifySequence({"win", "r"}, {{VK_LWIN, true}, {'R', false}}, "Win+R");
    verifySequence({"leftctrl", "leftshift", "a"},
                   {{VK_LCONTROL, false}, {VK_LSHIFT, false}, {'A', false}},
                   "Left modifiers");
    verifySequence({"rightctrl", "rightshift", "a"},
                   {{VK_RCONTROL, true}, {VK_RSHIFT, false}, {'A', false}},
                   "Right modifiers");
    verifySequence({"rightalt", "a"}, {{VK_RMENU, true}, {'A', false}}, "Right Alt");
    verifySequence({"shift+plus"},
                   {{VK_SHIFT, false}, {VK_OEM_PLUS, false}},
                   "Shift+plus expression");

    const auto rightControl = KeyboardInputWindows::parseKey("rightctrl");
    const auto rightAlt = KeyboardInputWindows::parseKey("rightalt");
    const auto rightShift = KeyboardInputWindows::parseKey("rightshift");
    require(rightControl && rightControl->extended, "Right Ctrl must be extended.");
    require(rightAlt && rightAlt->extended, "Right Alt must be extended.");
    require(rightShift && !rightShift->extended, "Right Shift must not be extended.");

    std::vector<INPUT> invalidDownInputs;
    std::vector<INPUT> invalidUpInputs;
    std::string invalidError;
    require(!KeyboardInputWindows::buildHotkeyInputs(
                {"ctrl++a"}, layout, invalidDownInputs, invalidUpInputs, invalidError),
            "An empty plus-delimited key name must be rejected.");
    require(invalidError.find("empty key names") != std::string::npos,
            "The empty-key diagnostic is missing.");

    if (failures != 0) {
        std::cerr << failures << " keyboard input test(s) failed.\n";
        return 1;
    }
    std::cout << "All keyboard input tests passed.\n";
    return 0;
}
