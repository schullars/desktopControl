#include "private/desktopAutomationPrivate.h"
#include "platform/windows/private/keyboardInputWindows.h"

#include <Windows.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <thread>

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

OperationResult DesktopAutomationPrivate::keyDown(const std::string& key)
{
    const std::optional<KeyboardInputWindows::KeyDefinition> definition
        = KeyboardInputWindows::parseKey(key);
    if (!definition) {
        return {false, "Unknown key: " + key};
    }
    INPUT input = KeyboardInputWindows::createInput(
        *definition, true, KeyboardInputWindows::foregroundKeyboardLayout());
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        return windowsFailure("SendInput(key down)");
    }
    return {true, "Key pressed."};
}

OperationResult DesktopAutomationPrivate::keyUp(const std::string& key)
{
    const std::optional<KeyboardInputWindows::KeyDefinition> definition
        = KeyboardInputWindows::parseKey(key);
    if (!definition) {
        return {false, "Unknown key: " + key};
    }
    INPUT input = KeyboardInputWindows::createInput(
        *definition, false, KeyboardInputWindows::foregroundKeyboardLayout());
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        return windowsFailure("SendInput(key up)");
    }
    return {true, "Key released."};
}

OperationResult DesktopAutomationPrivate::pressKey(const std::string& key)
{
    const std::optional<KeyboardInputWindows::KeyDefinition> definition
        = KeyboardInputWindows::parseKey(key);
    if (!definition) {
        return {false, "Unknown key: " + key};
    }

    const HKL layout = KeyboardInputWindows::foregroundKeyboardLayout();
    INPUT downInput = KeyboardInputWindows::createInput(*definition, true, layout);
    if (SendInput(1, &downInput, sizeof(INPUT)) != 1) {
        return windowsFailure("SendInput(key down)");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    INPUT upInput = KeyboardInputWindows::createInput(*definition, false, layout);
    if (SendInput(1, &upInput, sizeof(INPUT)) != 1) {
        return windowsFailure("SendInput(key up)");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    return {true, "Key pressed and released."};
}

OperationResult DesktopAutomationPrivate::pressHotkey(const std::vector<std::string>& keys)
{
    std::vector<INPUT> downInputs;
    std::vector<INPUT> upInputs;
    std::string error;
    if (!KeyboardInputWindows::buildHotkeyInputs(
            keys, KeyboardInputWindows::foregroundKeyboardLayout(), downInputs, upInputs, error)) {
        return {false, error};
    }

    std::size_t pressedCount = 0;
    for (INPUT& downInput : downInputs) {
        if (SendInput(1, &downInput, sizeof(INPUT)) != 1) {
            const DWORD failure = GetLastError();
            const auto firstRelease = upInputs.end() - static_cast<std::ptrdiff_t>(pressedCount);
            for (auto cleanupInput = firstRelease; cleanupInput != upInputs.end(); ++cleanupInput) {
                SendInput(1, &*cleanupInput, sizeof(INPUT));
            }
            SetLastError(failure);
            return windowsFailure("SendInput(hotkey key down)");
        }
        ++pressedCount;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bool releaseFailed = false;
    DWORD releaseFailure = ERROR_SUCCESS;
    for (INPUT& upInput : upInputs) {
        if (SendInput(1, &upInput, sizeof(INPUT)) != 1 && !releaseFailed) {
            releaseFailed = true;
            releaseFailure = GetLastError();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (releaseFailed) {
        SetLastError(releaseFailure);
        return windowsFailure("SendInput(hotkey key up)");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return {true, "Hotkey pressed."};
}

OperationResult DesktopAutomationPrivate::typeText(const std::string& text, TextInputMode mode)
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
    const HKL layout = KeyboardInputWindows::foregroundKeyboardLayout();
    const bool capsLockEnabled = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
    for (const wchar_t codeUnit : utf16) {
        std::vector<INPUT> downInputs;
        std::vector<INPUT> upInputs;
        const bool hasPhysicalMapping = mode != TextInputMode::Unicode
            && KeyboardInputWindows::buildCharacterInputs(
                codeUnit, layout, capsLockEnabled, downInputs, upInputs);
        if (hasPhysicalMapping) {
            std::size_t pressedCount = 0;
            for (INPUT& downInput : downInputs) {
                if (SendInput(1, &downInput, sizeof(INPUT)) != 1) {
                    const DWORD failure = GetLastError();
                    const auto firstRelease
                        = upInputs.end() - static_cast<std::ptrdiff_t>(pressedCount);
                    for (auto cleanupInput = firstRelease; cleanupInput != upInputs.end();
                         ++cleanupInput) {
                        SendInput(1, &*cleanupInput, sizeof(INPUT));
                    }
                    SetLastError(failure);
                    return windowsFailure("SendInput(physical text key down)");
                }
                ++pressedCount;
                std::this_thread::sleep_for(std::chrono::milliseconds(3));
            }

            bool releaseFailed = false;
            DWORD releaseFailure = ERROR_SUCCESS;
            for (INPUT& upInput : upInputs) {
                if (SendInput(1, &upInput, sizeof(INPUT)) != 1 && !releaseFailed) {
                    releaseFailed = true;
                    releaseFailure = GetLastError();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(3));
            }
            if (releaseFailed) {
                SetLastError(releaseFailure);
                return windowsFailure("SendInput(physical text key up)");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
            continue;
        }
        if (mode == TextInputMode::Physical) {
            return {false,
                    "Physical text input cannot represent UTF-16 code unit "
                        + std::to_string(static_cast<unsigned int>(codeUnit)) + "."};
        }

        INPUT down{};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = static_cast<WORD>(codeUnit);
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        if (SendInput(1, &down, sizeof(INPUT)) != 1) {
            return windowsFailure("SendInput(Unicode key down)");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        INPUT up{};
        up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        if (SendInput(1, &up, sizeof(INPUT)) != 1) {
            return windowsFailure("SendInput(Unicode key up)");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
    return {true, "Text typed."};
}

} // namespace desktopctl
