#include "commandLineApplicationPrivate.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <iostream>
#include <limits>

namespace desktopctl {

int CommandLineApplicationPrivate::run(const std::vector<std::string>& arguments)
{
    if (arguments.size() < 2) {
        printUsage();
        return 2;
    }

    const std::string command = lowerCopy(arguments[1]);
    if (command == "--help" || command == "-h" || command == "help") {
        printUsage();
        return 0;
    }

    if (command == "session") {
        const std::string action = arguments.size() == 3 ? lowerCopy(arguments[2]) : "start";
        if (action == "start" || action == "host") {
            return runSession(arguments);
        }
    }

    if (!activityIndicator_.isRunning()) {
        std::cerr << "Session is not started. Run \"desktopctl session start\" before using "
                     "other commands.\n";
        return 3;
    }
    if (!activityIndicator_.recordActivity().success) {
        std::cerr << "Session is not started. Run \"desktopctl session start\" before using "
                     "other commands.\n";
        return 3;
    }

    if (command == "--version" || command == "version") {
        std::cout << "desktopctl " DESKTOPCTL_VERSION "\n";
        return 0;
    }
    if (command == "mouse") {
        return runMouse(arguments);
    }
    if (command == "keyboard") {
        return runKeyboard(arguments);
    }
    if (command == "screenshot") {
        return runScreenshot(arguments);
    }
    if (command == "session") {
        return runSession(arguments);
    }
    if (command == "app") {
        return runApplication(arguments);
    }
    if (command == "window") {
        return runWindow(arguments);
    }

    std::cerr << "Unknown command: " << arguments[1] << '\n';
    printUsage();
    return 2;
}

std::string CommandLineApplicationPrivate::lowerCopy(const std::string& value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

std::optional<int> CommandLineApplicationPrivate::parseInteger(const std::string& value)
{
    int result = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto conversion = std::from_chars(begin, end, result);
    if (conversion.ec != std::errc() || conversion.ptr != end) {
        return std::nullopt;
    }
    return result;
}

std::optional<MouseButton> CommandLineApplicationPrivate::parseMouseButton(
    const std::string& value)
{
    const std::string button = lowerCopy(value);
    if (button == "left") {
        return MouseButton::Left;
    }
    if (button == "right") {
        return MouseButton::Right;
    }
    if (button == "middle") {
        return MouseButton::Middle;
    }
    return std::nullopt;
}

std::string CommandLineApplicationPrivate::escapeJson(const std::string& value)
{
    static constexpr char HexDigits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) {
        switch (character) {
        case '\"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (character < 0x20U) {
                result += "\\u00";
                result.push_back(HexDigits[(character >> 4U) & 0x0FU]);
                result.push_back(HexDigits[character & 0x0FU]);
            } else {
                result.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    return result;
}

void CommandLineApplicationPrivate::printUsage()
{
    std::cout << "desktopctl " DESKTOPCTL_VERSION R"HELP( - desktop automation CLI

Usage:
  desktopctl <command> [arguments]

General options:
  -H, -h, --help, help
      Show this complete help text. Help is the only command that never requires a session.
  --version, version
      Show the program version. Like every non-help command, this requires a started session.

Plugin transport:
  desktopctl mcp serve
      Run the local Model Context Protocol server over standard input and standard output. This
      transport entry point is intended for the packaged Codex desktop plugin and is not a desktop
      operation by itself. Its session_start tool must still be called before every other desktop
      tool, so the green indicator remains mandatory. Protocol output is newline-delimited UTF-8
      JSON-RPC and must not be mixed with manual console input.

Mandatory session gate:
  desktopctl session start
      Start the green activity indicator in a detached background process and return immediately.
      Run this before every other non-help command. Repeating start is safe and reports that the
      session is already running. Every accepted non-help command refreshes a three-minute
      inactivity deadline.
  desktopctl session stop
      Stop the background indicator, close all overlay windows, and clear the session state.

  Any command other than help or session start is rejected with exit code 3 when no session is
  active. Session state is owned by Windows named kernel objects, not a configuration file. If the
  background host exits or crashes, Windows releases the state automatically. The background host
  also exits automatically after three minutes without a non-help command. Help does not start or
  refresh a session. After an inactivity timeout, run session start again before another command.

  Recommended PowerShell lifecycle:
      $tool = ".\desktopctl.exe"
      & $tool session start
      if ($LASTEXITCODE -ne 0) { throw "Session start failed" }
      try {
          & $tool --version
          # Run mouse, keyboard, screenshot, window, and app commands here.
      } finally {
          & $tool session stop
      }

Mouse control:
  desktopctl mouse move X Y
      Move to absolute physical-pixel coordinates on the virtual desktop. Coordinates may be
      negative on multi-monitor desktops.
  desktopctl mouse move-relative DX DY
      Move relative to the current pointer position in pixels.
  desktopctl mouse click BUTTON [COUNT]
      Click left, right, or middle. COUNT defaults to 1 and must be from 1 through 100.
  desktopctl mouse down BUTTON
  desktopctl mouse up BUTTON
      Press or release a button separately. Always pair down with a later up.
  desktopctl mouse wheel DELTA [vertical|horizontal]
      Scroll the wheel. One Windows notch is 120; negative values reverse direction.

Keyboard control:
  desktopctl keyboard press KEY
      Press and release one key.
  desktopctl keyboard down KEY
  desktopctl keyboard up KEY
      Press or release a key separately. Always pair down with a later up.
  desktopctl keyboard hotkey KEY [KEY...]
      Press keys in order, hold the complete combination briefly, and release in reverse order.
      Pass separate names or one plus-delimited expression, for example ctrl shift escape or
      ctrl+shift+escape. Hotkeys follow the active target window's keyboard layout.
  desktopctl keyboard type TEXT
      Type UTF-8 text into the focused window. Characters available in the active keyboard layout
      use physical scan codes; other characters fall back to Unicode input.
  desktopctl keyboard type-physical TEXT
      Require physical scan-code input. This is the most compatible mode for virtual machines,
      remote desktops, ASCII text, symbols, and passwords. The target layout and unsynchronized
      remote lock-key state can affect the result. Unrepresentable characters return an error.
  desktopctl keyboard type-unicode TEXT
      Force Unicode input. Use this for direct non-ASCII text in compatible local applications.
      Some virtual-machine and remote-desktop clients do not forward Unicode packet events.

  Text input mode selection:
      1. Start with keyboard type for ordinary text. Hybrid mode maps every character available in
         the foreground Windows keyboard layout to a physical key chord and falls back to Unicode
         only for a character that has no physical mapping.
      2. Use type-physical when Unicode fallback must never occur, especially for an ASCII login,
         symbol, or password field inside a virtual machine or remote desktop. The command returns
         an error if any character cannot be represented by the foreground keyboard layout.
      3. Use type-unicode only for a compatible local application. It sends Unicode packet events;
         it does not simulate the physical Shift, Ctrl, or Alt chord for the character.

      With a US keyboard layout, hybrid and physical modes generate @ as Shift+2 and # as Shift+3.
      Do not send those modifier combinations manually when TEXT already contains @ or #. The
      foreground Windows layout and the guest or remote layout must agree. DesktopCTL does not
      install, select, or synchronize a guest input method or keyboard layout.

      If ASCII letter case is inverted while symbols are correct, the host and guest Caps Lock
      states probably differ. Do not press capslock while the virtual machine or remote desktop is
      focused because that can toggle both states and preserve the mismatch. Activate a local
      Windows window outside the client, press capslock once to change only the host state, then
      reactivate the target, select the complete field, retype it, and capture another screenshot.
      Do not submit until the screenshot shows the exact intended case and symbols.

      Command success means Windows accepted the generated input events. It cannot prove that a
      virtual machine, remote client, browser, or input method produced the intended text. Activate
      the exact target, place the caret, enter the complete field, and verify the result with a
      target-window screenshot before submitting a login or another irreversible action.

      Plugin keyboard_control actions use type, type_physical, and type_unicode. The equivalent CLI
      names are type, type-physical, and type-unicode.

  Common keys:
      a-z, 0-9, f1-f24, enter, tab, space, escape, backspace,
      ctrl, shift, alt, win, left, right, up, down, home, end,
      pageup, pagedown, insert, delete, printscreen, numpad0-numpad9.

Screenshots:
  desktopctl screenshot
  desktopctl screenshot full [OUTPUT.jpg]
      Capture the virtual desktop. Without OUTPUT, save under the current user's system temporary
      directory in DesktopCTL\screenshots using a local timestamp, for example
      screenshot_20261029_173045_123.jpg.
  desktopctl screenshot region X Y WIDTH HEIGHT [OUTPUT.jpg]
      Capture a physical-pixel region fully contained in the virtual desktop.
  desktopctl screenshot window "ID_OR_NAME" [OUTPUT.jpg]
      Capture one application window by ID, exact title, or unique process name. Quote names
      containing spaces. Without OUTPUT, use a timestamped window_*.jpg name.
  desktopctl screenshot OUTPUT_IMAGE [X Y WIDTH HEIGHT]
      Legacy-compatible syntax. .jpg and .jpeg use JPEG quality 95; .bmp remains available for
      uncompressed 32-bit output. The extension selects the encoder.

  Default format policy:
      Use JPG for all routine screenshots unless lossless source pixels are specifically required.
      Omitting OUTPUT automatically creates a quality-95 JPG. JPG uses far less disk space and
      transfer bandwidth. Use BMP only for lossless archival, exact pixel inspection, or another
      workflow that explicitly requires uncompressed image data.

Window discovery and activation:
  desktopctl window list
      Write UTF-8 JSON containing id, title, processId, processName, minimized, active, and
      physical-pixel bounds for visible titled top-level windows. IDs remain valid only while
      their windows exist. active=true identifies the foreground window at enumeration time.
  desktopctl window activate "ID_OR_NAME"
      Restore a minimized target, request foreground activation, and verify the result. Use an
      ID when a title or process name is ambiguous. Success writes the title and ID to stdout.
      A denied foreground request writes an error to stderr and returns exit code 3.

Screenshot JSON result:
  A successful capture returns exit code 0 and writes only one UTF-8 JSON object to stdout:
      {"success":true,"path":"D:\\...\\screenshot.jpg","directory":"D:\\...",
       "imageCount":51,"cleanupRecommended":true,
       "message":"Screenshot saved. The output directory contains 51 image file(s). More than 50 images are present; remove screenshots you no longer need."}

  path
      Absolute path of the generated image. Actual filesystem characters are preserved.
  directory
      Absolute parent directory. Actual filesystem characters are preserved.
  imageCount
      Number of .jpg, .jpeg, and .bmp files in that directory, or null if counting failed.
  cleanupRecommended
      true only when imageCount is available and greater than 50.
  message
      English capture status and cleanup guidance.

  The default directory is %TEMP%\DesktopCTL\screenshots on Windows, not the executable or current
  shell directory. The stable directory is reused across commands so earlier images remain in the
  count. If the operating system removes it, the next default capture creates it again.
  Relative OUTPUT paths are resolved from the current directory, but JSON paths are absolute.
  A system capture failure writes success=false JSON to stderr and returns exit code 3.
  Invalid command arguments write an error to stderr and return exit code 2.

  PowerShell parsing example:
      $json = & $tool screenshot
      if ($LASTEXITCODE -ne 0) { throw "Screenshot failed" }
      $result = $json | ConvertFrom-Json
      Write-Host "Path: $($result.path); image count: $($result.imageCount)"
      if ($result.cleanupRecommended) { Write-Warning $result.message }

Activity indicator behavior:
  session start returns only after the green gradient border and pointer halo are ready. The
  detached host remains alive across later desktopctl invocations. Each accepted non-help command
  resets its inactivity deadline. After three minutes with no such command, the host exits and the
  overlays close automatically. session stop performs the same cleanup immediately. Only one
  instance is allowed per login session. Each active display receives its own complete border,
  including displays with negative coordinates, unequal sizes, or misaligned and L-shaped layouts.
  Connecting, disconnecting, or rearranging displays rebuilds the borders automatically.

Application discovery and launch:
  desktopctl app list
      Write UTF-8 JSON for launchable items in the current user's Windows AppsFolder. Each item
      has a stable id and display name and may represent a desktop or Microsoft Store app.
  desktopctl app open ID_OR_NAME
      Open an item by an ID returned from app list or by a unique exact display name. Quote names
      containing spaces. Use the ID when a name is ambiguous.

Foreground focus rules:
  1. Mouse and keyboard commands operate on the current interactive desktop. Keyboard input is
     sent to the current foreground window.
  2. app open requests launch or activation, but an application may reuse an existing window or
     create a new one.
  3. Use window list to obtain an ID, then window activate before sending keyboard input.
  4. Place the caret explicitly before typing. Use ctrl+a before replacing content, end or
     ctrl+end before appending, and home or ctrl+home before inserting at the beginning.
  5. keyboard type uses hybrid physical/Unicode input. Use type-physical for virtual machines,
     remote desktops, symbols, and passwords. Physical input follows the target layout. The host
     Caps Lock state is compensated, but an unsynchronized remote lock state can still affect case.
     Use type-unicode only when direct Unicode packet input is supported.
  6. For browsers, ctrl+n creates a window and ctrl+l focuses its address bar.
  7. keyboard type does not press Enter; use keyboard press enter when navigation is required.
  8. Add short waits after application launch, window creation, and page navigation.
  9. Do not enter a cross-window sequence one command at a time: clicking the terminal changes
     foreground focus. Paste the complete script or run it from a file.
  10. Verify active, processName, title, caret placement, and window count afterward. Use IDs for
      duplicate names and capture a screenshot when the resulting UI state matters.

Virtual-machine text workflow (PowerShell):
  $tool = ".\desktopctl.exe"
  $target = "VM_WINDOW_ID_OR_UNIQUE_NAME"
  & $tool session start
  if ($LASTEXITCODE -ne 0) { throw "Session start failed" }
  try {
      & $tool window activate $target
      if ($LASTEXITCODE -ne 0) { throw "VM activation failed" }
      & $tool keyboard hotkey ctrl+a
      & $tool keyboard type-physical "admin@#"
      if ($LASTEXITCODE -ne 0) { throw "Physical text input failed" }
      $capture = & $tool screenshot window $target | ConvertFrom-Json
      Write-Host "Verify the field in: $($capture.path)"
      # Submit only after the screenshot confirms the complete text.
  } finally {
      & $tool session stop
  }

  Use keyboard type instead of type-physical when the text may contain characters unavailable in
  the foreground physical layout and the target is known to accept Unicode packet input.
  If ASCII letter case is inverted, activate a local Windows window outside the VM client, run
  keyboard press capslock once to change only the host state, reactivate the VM, repeat ctrl+a and
  type-physical for the complete field, and capture a new screenshot before submitting.

Browser workflow (PowerShell):
  $tool = ".\desktopctl.exe"
  & $tool session start
  if ($LASTEXITCODE -ne 0) { throw "Session start failed" }
  try {
      & $tool app open "Google Chrome"
      if ($LASTEXITCODE -ne 0) { throw "Chrome launch failed" }
      Start-Sleep -Milliseconds 800
      & $tool keyboard hotkey ctrl n
      Start-Sleep -Milliseconds 700
      $windows = (& $tool window list | ConvertFrom-Json).windows
      $target = $windows | Where-Object processName -ieq "chrome.exe" |
          Where-Object active | Select-Object -First 1
      if (-not $target) { throw "No active Chrome window was found" }
      & $tool window activate $target.id
      if ($LASTEXITCODE -ne 0) { throw "Chrome activation failed" }
      & $tool keyboard hotkey ctrl l
      & $tool keyboard type "https://youtube.com"
      & $tool keyboard press enter
      Start-Sleep -Milliseconds 1500
      & $tool window list
  } finally {
      & $tool session stop
  }

  Replace the URL with https://google.com to open Google. If multiple Chrome windows exist, use
  the selected ID with both commands below:
      desktopctl window activate "0xWINDOW_ID"
      desktopctl screenshot window "0xWINDOW_ID"

Exit codes:
  0  Success
  2  Invalid command or arguments
  3  Platform operation failure or operation not implemented on this platform

Examples:
  desktopctl session start
  desktopctl mouse move 800 450
  desktopctl mouse click left 2
  desktopctl mouse wheel -120 vertical
  desktopctl keyboard hotkey ctrl shift s
  desktopctl keyboard hotkey ctrl+shift+escape
  desktopctl keyboard type "@"
  desktopctl keyboard type-physical "@#"
  desktopctl keyboard type-unicode "Hello, world"
  desktopctl keyboard type "Hello, world"
  desktopctl screenshot desktop.jpg
  desktopctl screenshot region 0 0 1280 720
  desktopctl window list > windows.json
  desktopctl window activate "0x00012345"
  desktopctl screenshot window "notepad.exe"
  desktopctl app list > applications.json
  desktopctl app open "Calculator"
  desktopctl app open "app:Microsoft.WindowsCalculator_8wekyb3d8bbwe!App"
  desktopctl session stop

Limitations:
  Input targets the interactive desktop and focused window. A normal process cannot control a
  higher-integrity window, the UAC secure desktop, or a locked screen. Confirm target state before
  click, type, hotkey, and app open operations.
)HELP";
}

int CommandLineApplicationPrivate::reportResult(const OperationResult& result)
{
    if (!result.success) {
        std::cerr << result.message << '\n';
        return 3;
    }
    if (!result.message.empty()) {
        std::cout << result.message << '\n';
    }
    return 0;
}

int CommandLineApplicationPrivate::reportScreenshotResult(const ScreenshotResult& result)
{
    const bool success = result.operation.success;
    std::ostream& output = success ? std::cout : std::cerr;
    output << "{\"success\": " << (success ? "true" : "false") << ", \"path\": ";
    if (success) {
        output << '"' << escapeJson(result.path) << '"';
    } else {
        output << "null";
    }
    output << ", \"directory\": ";
    if (success) {
        output << '"' << escapeJson(result.directory) << '"';
    } else {
        output << "null";
    }
    output << ", \"imageCount\": ";
    if (result.imageCount) {
        output << *result.imageCount;
    } else {
        output << "null";
    }
    const std::string message = success ? result.message : result.operation.message;
    output << ", \"cleanupRecommended\": "
           << (result.cleanupRecommended ? "true" : "false")
           << ", \"message\": \"" << escapeJson(message) << "\"}\n";
    return success ? 0 : 3;
}

int CommandLineApplicationPrivate::runMouse(const std::vector<std::string>& arguments)
{
    if (arguments.size() < 3) {
        std::cerr << "Missing mouse action.\n";
        return 2;
    }

    const std::string action = lowerCopy(arguments[2]);
    if (action == "move" || action == "move-relative") {
        if (arguments.size() != 5) {
            std::cerr << "Mouse move requires two integer coordinates.\n";
            return 2;
        }
        const std::optional<int> x = parseInteger(arguments[3]);
        const std::optional<int> y = parseInteger(arguments[4]);
        if (!x || !y) {
            std::cerr << "Mouse coordinates must be valid 32-bit integers.\n";
            return 2;
        }
        return reportResult(action == "move" ? automation_.moveMouse(*x, *y)
                                             : automation_.moveMouseRelative(*x, *y));
    }

    if (action == "click") {
        if (arguments.size() != 4 && arguments.size() != 5) {
            std::cerr << "Mouse click requires BUTTON and optional COUNT.\n";
            return 2;
        }
        const std::optional<MouseButton> button = parseMouseButton(arguments[3]);
        if (!button) {
            std::cerr << "BUTTON must be left, right, or middle.\n";
            return 2;
        }
        int count = 1;
        if (arguments.size() == 5) {
            const std::optional<int> parsedCount = parseInteger(arguments[4]);
            if (!parsedCount || *parsedCount < 1 || *parsedCount > 100) {
                std::cerr << "COUNT must be between 1 and 100.\n";
                return 2;
            }
            count = *parsedCount;
        }
        return reportResult(automation_.clickMouse(*button, count));
    }

    if (action == "down" || action == "up") {
        if (arguments.size() != 4) {
            std::cerr << "Mouse down/up requires exactly one BUTTON.\n";
            return 2;
        }
        const std::optional<MouseButton> button = parseMouseButton(arguments[3]);
        if (!button) {
            std::cerr << "BUTTON must be left, right, or middle.\n";
            return 2;
        }
        return reportResult(action == "down" ? automation_.mouseDown(*button)
                                             : automation_.mouseUp(*button));
    }

    if (action == "wheel") {
        if (arguments.size() != 4 && arguments.size() != 5) {
            std::cerr << "Mouse wheel requires DELTA and optional direction.\n";
            return 2;
        }
        const std::optional<int> delta = parseInteger(arguments[3]);
        if (!delta) {
            std::cerr << "DELTA must be a valid 32-bit integer.\n";
            return 2;
        }
        WheelDirection direction = WheelDirection::Vertical;
        if (arguments.size() == 5) {
            const std::string directionName = lowerCopy(arguments[4]);
            if (directionName == "horizontal") {
                direction = WheelDirection::Horizontal;
            } else if (directionName != "vertical") {
                std::cerr << "Wheel direction must be vertical or horizontal.\n";
                return 2;
            }
        }
        return reportResult(automation_.scrollMouse(*delta, direction));
    }

    std::cerr << "Unknown mouse action: " << arguments[2] << '\n';
    return 2;
}

int CommandLineApplicationPrivate::runKeyboard(const std::vector<std::string>& arguments)
{
    if (arguments.size() < 3) {
        std::cerr << "Missing keyboard action.\n";
        return 2;
    }

    const std::string action = lowerCopy(arguments[2]);
    if (action == "press" || action == "down" || action == "up") {
        if (arguments.size() != 4) {
            std::cerr << "Keyboard press/down/up requires exactly one KEY.\n";
            return 2;
        }
        if (action == "press") {
            return reportResult(automation_.pressKey(arguments[3]));
        }
        return reportResult(action == "down" ? automation_.keyDown(arguments[3])
                                             : automation_.keyUp(arguments[3]));
    }

    if (action == "hotkey") {
        if (arguments.size() < 4) {
            std::cerr << "Keyboard hotkey requires at least one KEY.\n";
            return 2;
        }
        const std::vector<std::string> keys(arguments.begin() + 3, arguments.end());
        return reportResult(automation_.pressHotkey(keys));
    }

    if (action == "type" || action == "type-physical" || action == "type-unicode") {
        if (arguments.size() < 4) {
            std::cerr << "Keyboard type/type-physical/type-unicode requires TEXT.\n";
            return 2;
        }
        std::string text = arguments[3];
        for (std::size_t index = 4; index < arguments.size(); ++index) {
            text += ' ';
            text += arguments[index];
        }
        TextInputMode mode = TextInputMode::Hybrid;
        if (action == "type-physical") {
            mode = TextInputMode::Physical;
        } else if (action == "type-unicode") {
            mode = TextInputMode::Unicode;
        }
        return reportResult(automation_.typeText(text, mode));
    }

    std::cerr << "Unknown keyboard action: " << arguments[2] << '\n';
    return 2;
}

int CommandLineApplicationPrivate::runScreenshot(const std::vector<std::string>& arguments)
{
    if (arguments.size() == 2) {
        return reportScreenshotResult(screenCapture_.captureDesktop());
    }

    const std::string action = lowerCopy(arguments[2]);
    if (action == "full") {
        if (arguments.size() != 3 && arguments.size() != 4) {
            std::cerr << "Screenshot full accepts only an optional OUTPUT_IMAGE.\n";
            return 2;
        }
        const std::optional<std::string> output = arguments.size() == 4
            ? std::optional<std::string>(arguments[3])
            : std::nullopt;
        return reportScreenshotResult(screenCapture_.captureDesktop(output));
    }

    if (action == "region") {
        if (arguments.size() != 7 && arguments.size() != 8) {
            std::cerr << "Screenshot region requires X Y WIDTH HEIGHT and optional OUTPUT_IMAGE.\n";
            return 2;
        }
        const std::optional<int> x = parseInteger(arguments[3]);
        const std::optional<int> y = parseInteger(arguments[4]);
        const std::optional<int> width = parseInteger(arguments[5]);
        const std::optional<int> height = parseInteger(arguments[6]);
        if (!x || !y || !width || !height || *width <= 0 || *height <= 0) {
            std::cerr << "Screenshot region must contain valid integers and positive dimensions.\n";
            return 2;
        }
        const std::optional<std::string> output = arguments.size() == 8
            ? std::optional<std::string>(arguments[7])
            : std::nullopt;
        return reportScreenshotResult(
            screenCapture_.captureRegion(ScreenRegion{*x, *y, *width, *height}, output));
    }

    if (action == "window") {
        if (arguments.size() != 4 && arguments.size() != 5) {
            std::cerr << "Screenshot window requires a quoted ID_OR_NAME and optional OUTPUT_IMAGE.\n";
            return 2;
        }
        const std::optional<std::string> output = arguments.size() == 5
            ? std::optional<std::string>(arguments[4])
            : std::nullopt;
        return reportScreenshotResult(screenCapture_.captureWindow(arguments[3], output));
    }

    if (arguments.size() == 3) {
        return reportScreenshotResult(screenCapture_.captureDesktop(arguments[2]));
    }
    if (arguments.size() == 7) {
        const std::optional<int> x = parseInteger(arguments[3]);
        const std::optional<int> y = parseInteger(arguments[4]);
        const std::optional<int> width = parseInteger(arguments[5]);
        const std::optional<int> height = parseInteger(arguments[6]);
        if (!x || !y || !width || !height || *width <= 0 || *height <= 0) {
            std::cerr << "Screenshot region must contain valid integers and positive dimensions.\n";
            return 2;
        }
        return reportScreenshotResult(screenCapture_.captureRegion(
            ScreenRegion{*x, *y, *width, *height}, arguments[2]));
    }

    std::cerr << "Invalid screenshot command. Run desktopctl -H for examples.\n";
    return 2;
}

int CommandLineApplicationPrivate::runSession(const std::vector<std::string>& arguments)
{
    if (arguments.size() > 3) {
        std::cerr << "Session accepts only start or stop.\n";
        return 2;
    }

    const std::string action = arguments.size() == 3 ? lowerCopy(arguments[2]) : "start";
    if (action == "stop") {
        return reportResult(activityIndicator_.requestStop());
    }
    if (action == "host") {
        return reportResult(activityIndicator_.run());
    }
    if (action != "start") {
        std::cerr << "Session action must be start or stop.\n";
        return 2;
    }

    return reportResult(activityIndicator_.startBackground());
}

int CommandLineApplicationPrivate::runApplication(
    const std::vector<std::string>& arguments)
{
    if (arguments.size() < 3) {
        std::cerr << "Missing app action.\n";
        return 2;
    }

    const std::string action = lowerCopy(arguments[2]);
    if (action == "list") {
        if (arguments.size() != 3 && (arguments.size() != 4 || arguments[3] != "--json")) {
            std::cerr << "App list accepts only the optional --json flag.\n";
            return 2;
        }
        const ApplicationListResult listing = applicationCatalog_.listApplications();
        if (!listing.operation.success) {
            return reportResult(listing.operation);
        }
        std::cout << "{\n  \"applications\": [\n";
        for (std::size_t index = 0; index < listing.applications.size(); ++index) {
            const ApplicationInfo& application = listing.applications[index];
            std::cout << "    {\"id\": \"" << escapeJson(application.id)
                      << "\", \"name\": \"" << escapeJson(application.name) << "\"}"
                      << (index + 1U == listing.applications.size() ? "\n" : ",\n");
        }
        std::cout << "  ]\n}\n";
        return 0;
    }

    if (action == "open") {
        if (arguments.size() < 4) {
            std::cerr << "App open requires an application ID or exact name.\n";
            return 2;
        }
        std::string query = arguments[3];
        for (std::size_t index = 4; index < arguments.size(); ++index) {
            query += ' ';
            query += arguments[index];
        }
        return reportResult(applicationCatalog_.openApplication(query));
    }

    std::cerr << "Unknown app action: " << arguments[2] << '\n';
    return 2;
}

int CommandLineApplicationPrivate::runWindow(const std::vector<std::string>& arguments)
{
    if (arguments.size() < 3) {
        std::cerr << "Missing window action. Use list or activate.\n";
        return 2;
    }

    const std::string action = lowerCopy(arguments[2]);
    if (action == "list") {
        if (arguments.size() != 3) {
            std::cerr << "Window list does not accept additional arguments.\n";
            return 2;
        }
        const WindowListResult listing = screenCapture_.listWindows();
        if (!listing.operation.success) {
            return reportResult(listing.operation);
        }
        std::cout << "{\n  \"windows\": [\n";
        for (std::size_t index = 0; index < listing.windows.size(); ++index) {
            const WindowInfo& window = listing.windows[index];
            std::cout << "    {\"id\": \"" << escapeJson(window.id) << "\", \"title\": \""
                      << escapeJson(window.title) << "\", \"processId\": " << window.processId
                      << ", \"processName\": \"" << escapeJson(window.processName)
                      << "\", \"minimized\": " << (window.minimized ? "true" : "false")
                      << ", \"active\": " << (window.active ? "true" : "false")
                      << ", \"x\": " << window.x << ", \"y\": " << window.y
                      << ", \"width\": " << window.width << ", \"height\": " << window.height
                      << '}'
                      << (index + 1U == listing.windows.size() ? "\n" : ",\n");
        }
        std::cout << "  ]\n}\n";
        return 0;
    }

    if (action == "activate") {
        if (arguments.size() < 4) {
            std::cerr << "Window activate requires a window ID, exact title, or unique process name.\n";
            return 2;
        }
        std::string query = arguments[3];
        for (std::size_t index = 4; index < arguments.size(); ++index) {
            query += ' ';
            query += arguments[index];
        }
        return reportResult(screenCapture_.activateWindow(query));
    }

    std::cerr << "Unknown window action: " << arguments[2] << "\n";
    return 2;
}

} // namespace desktopctl
