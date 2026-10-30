#pragma once

#include <Windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace desktopctl {

class KeyboardInputWindows final {
public:
    struct KeyDefinition {
        std::uint16_t virtualKey = 0;
        bool extended = false;
    };

    static std::optional<KeyDefinition> parseKey(const std::string& key);
    static HKL foregroundKeyboardLayout();
    static INPUT createInput(const KeyDefinition& key, bool pressed, HKL layout);
    // Maps one character to a complete physical key chord for the supplied layout and lock state.
    static bool buildCharacterInputs(wchar_t character,
                                     HKL layout,
                                     bool capsLockEnabled,
                                     std::vector<INPUT>& downInputs,
                                     std::vector<INPUT>& upInputs);
    static bool buildHotkeyInputs(const std::vector<std::string>& keys,
                                  HKL layout,
                                  std::vector<INPUT>& downInputs,
                                  std::vector<INPUT>& upInputs,
                                  std::string& error);

private:
    static std::string normalizeKey(const std::string& key);
    static bool expandHotkeyKeys(const std::vector<std::string>& keys,
                                 std::vector<std::string>& expandedKeys,
                                 std::string& error);
};

} // namespace desktopctl
