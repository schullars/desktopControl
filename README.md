# desktopctl

`desktopctl` is a C++17 command-line utility for mouse and keyboard control, screenshots,
window discovery and activation, and application discovery and launch.

The Windows backend is implemented. The Ubuntu 22.04/Linux backend is connected to the same
interfaces and build system but currently returns explicit not-implemented errors. This keeps
the command protocol stable while future X11 and Wayland backends are added.

## Build

Windows with Visual Studio 2019:

```powershell
cmake -S . -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Windows Release uses the static MSVC runtime (`/MT`). Distribution requires only
`desktopctl.exe`; no Visual C++ runtime DLL, configuration file, or resource directory is
required. Windows system DLLs are still supplied by the operating system.

Ubuntu 22.04, for the architecture and placeholder backend:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Built-in help

Run any of the following commands for complete parameter rules and copyable examples:

```text
desktopctl -H
desktopctl -h
desktopctl --help
desktopctl help
```

## Command summary

```text
desktopctl mouse move X Y
desktopctl mouse move-relative DX DY
desktopctl mouse click BUTTON [COUNT]
desktopctl mouse down BUTTON
desktopctl mouse up BUTTON
desktopctl mouse wheel DELTA [vertical|horizontal]

desktopctl keyboard press KEY
desktopctl keyboard down KEY
desktopctl keyboard up KEY
desktopctl keyboard hotkey KEY [KEY...]
desktopctl keyboard type TEXT
desktopctl keyboard type-physical TEXT
desktopctl keyboard type-unicode TEXT

desktopctl screenshot OUTPUT_IMAGE [X Y WIDTH HEIGHT]
desktopctl screenshot
desktopctl screenshot full [OUTPUT.jpg]
desktopctl screenshot region X Y WIDTH HEIGHT [OUTPUT.jpg]
desktopctl screenshot window "ID_OR_NAME" [OUTPUT.jpg]

desktopctl window list
desktopctl window activate "ID_OR_NAME"

desktopctl session [start]
desktopctl session stop

desktopctl app list
desktopctl app open ID_OR_NAME

desktopctl mcp serve
```

## Mandatory session lifecycle

Help (`-H`, `-h`, `--help`, or `help`) is always available. Every desktop operation is rejected
with exit code `3` until `desktopctl session start` succeeds. This includes `--version`, mouse,
keyboard, screenshot, window, application, and session-stop commands. `desktopctl mcp serve` is an
internal protocol transport rather than a desktop operation; its exposed tools enforce the same
session gate.

`session start` launches the green activity indicator in a detached background process, waits
until the border and pointer halo are ready, and then returns immediately. Repeating the command
is safe and refreshes the inactivity deadline. Every accepted non-help command refreshes the same
deadline. After three minutes without a non-help command, the host exits automatically, closes all
overlays, and clears the state. Help never starts or refreshes a session. A later non-help command
is rejected with exit code `3` until `session start` is called again. `session stop` performs the
same cleanup immediately. The state is held by Windows named kernel objects rather than a file. If
the host exits or crashes, Windows automatically releases the state.

Use `try/finally` so the indicator closes when the calling workflow finishes or fails:

```powershell
$tool = ".\desktopctl.exe"
& $tool session start
if ($LASTEXITCODE -ne 0) { throw "Session start failed" }
try {
    & $tool --version
    # Run mouse, keyboard, screenshot, window, and app commands here.
} finally {
    & $tool session stop
}
```

Mouse buttons are `left`, `right`, and `middle`. One standard Windows wheel notch is `120`;
negative values reverse direction.

Key names include letters, digits, `F1` through `F24`, `enter`, `tab`, `space`, `escape`,
`ctrl`, `shift`, `alt`, `win`, navigation keys, editing keys, and numeric keypad keys. Use
`keyboard type` for hybrid physical/Unicode text and quote text that contains spaces.

## Screenshot output and cleanup guidance

Running `desktopctl screenshot` captures the virtual desktop and creates the stable
`%TEMP%\DesktopCTL\screenshots` directory for the current Windows user. This avoids requiring write
access beside the EXE when the tool is installed under `Program Files` or another protected
location. The same directory is reused by later commands, so its existing images remain part of
the cleanup count. If the operating system removes the temporary directory, the next default
capture creates it again. The default filename has the form
`screenshot_YYYYMMDD_HHMMSS_mmm.jpg`. Region coordinates are physical pixels in the virtual
desktop and may be negative on multi-monitor systems. JPEG is the default and uses quality 95.
Paths ending in `.jpg` or `.jpeg` use the JPEG encoder; `.bmp` remains available for uncompressed
32-bit output. The file extension selects the encoder.

Use JPG for all routine screenshots unless a workflow explicitly requires lossless source pixels.
Omitting the output path automatically creates a quality-95 JPG, which uses substantially less
disk space and transfer bandwidth. Use BMP only for lossless archival, exact pixel inspection, or
another requirement that specifically needs uncompressed image data.

Every successful screenshot returns exit code `0` and writes only one UTF-8 JSON object to
stdout:

```json
{
  "success": true,
  "path": "C:\\Users\\example\\AppData\\Local\\Temp\\DesktopCTL\\screenshots\\screenshot_20261029_180000_123.jpg",
  "directory": "C:\\Users\\example\\AppData\\Local\\Temp\\DesktopCTL\\screenshots",
  "imageCount": 51,
  "cleanupRecommended": true,
  "message": "Screenshot saved. The output directory contains 51 image file(s). More than 50 images are present; remove screenshots you no longer need."
}
```

- `path` is the absolute screenshot path.
- `directory` is the absolute parent directory.
- `imageCount` is the number of `.jpg`, `.jpeg`, and `.bmp` files in that directory after capture.
- `cleanupRecommended` is `true` only when `imageCount` is greater than 50.
- `message` is a stable English status and cleanup message.

If directory enumeration fails after a successful capture, `imageCount` is `null`,
`cleanupRecommended` is `false`, and `message` describes the count failure. A relative output
path is resolved from the current working directory, but the returned paths are absolute.

A system capture failure writes a `success: false` JSON object to stderr and returns exit code
`3`. Invalid command arguments write an error to stderr and return exit code `2`.

PowerShell parsing example:

```powershell
$tool = ".\desktopctl.exe"
$json = & $tool screenshot
if ($LASTEXITCODE -ne 0) { throw "Screenshot failed" }
$result = $json | ConvertFrom-Json
Write-Host "Path: $($result.path)"
Write-Host "Image count: $($result.imageCount)"
if ($result.cleanupRecommended) {
    Write-Warning $result.message
}
```

## Window discovery and activation

`window list` writes UTF-8 JSON containing transient window IDs, titles, process IDs, process
names, minimized state, foreground state in `active`, and physical-pixel bounds. A window ID is
valid only while that window exists.

`window activate "ID_OR_NAME"` restores a minimized target, requests top-level foreground
activation and keyboard focus, and verifies that the target became the foreground window. The
argument may be a window ID, exact title, or unique process name. Use an ID when a process owns
multiple windows or a title match is ambiguous.

Use `screenshot window` for a target-aware window capture by ID, exact title, or unique process
name. Without an output path it writes a timestamped `window_*.jpg` file.

## Activity indicator

`session start` runs a separate green gradient border around every active display and a
click-through green halo beside the system pointer. Negative coordinates, unequal display sizes,
and misaligned or L-shaped layouts are supported. Connecting, disconnecting, or rearranging
displays rebuilds the borders automatically. The command returns after the detached host is ready.
The host automatically closes the indicator after three minutes without an accepted non-help
command. Run `desktopctl session stop` to close it immediately. Only one indicator instance is
allowed in a login session.

## Keyboard combinations and literal text

Hotkeys accept separate key names or one plus-delimited expression. These two commands are
equivalent:

```text
desktopctl keyboard hotkey ctrl shift escape
desktopctl keyboard hotkey ctrl+shift+escape
```

DesktopCTL sends hotkeys as physical scan-code input using the foreground window's active keyboard
layout, holds the complete combination briefly, and releases keys in reverse order.

`keyboard type` is the recommended starting point for ordinary text. It is hybrid: each character
available in the foreground Windows layout uses the same physical scan-code path as a real keyboard,
while an unrepresentable character falls back to Unicode input. Use `type-physical` when Unicode
fallback must never occur, especially for virtual-machine or remote-desktop ASCII logins, symbols,
and passwords. It returns an error if any character lacks a physical mapping. Use `type-unicode`
only to force direct Unicode packet input in a compatible local application. Some virtual-machine
and remote-desktop clients do not forward Unicode packet events.

```text
desktopctl keyboard type "@"
desktopctl keyboard type-physical "@#"
desktopctl keyboard type-unicode "Hello, world"
```

With a US layout, hybrid and physical modes generate `@` as Shift+2 and `#` as Shift+3. Do not send
those hotkeys manually when the text already contains the symbols. Physical input is mapped from the
foreground Windows layout, while the guest or remote system interprets the received keys with its
own layout and input method. The layouts must agree; DesktopCTL does not install, select, or
synchronize a guest input method. DesktopCTL compensates for host Caps Lock when typing ASCII
letters, but an unsynchronized guest lock state can still affect case.

If ASCII letter case is inverted while symbols are correct, the host and guest Caps Lock states
probably differ. Do not press Caps Lock while the virtual machine or remote desktop is focused,
because that can toggle both states and preserve the mismatch. Activate a local Windows window
outside the client, run `desktopctl keyboard press capslock` once to change only the host state,
reactivate the target, select the complete field, retype it, and capture another screenshot. Do not
submit until the screenshot shows the exact intended case and symbols.

Command success means Windows accepted the generated input events. It does not prove that a virtual
machine, remote client, browser, or input method produced the intended text. Activate the exact
target, place the caret, enter the complete field, and verify it with a target-window screenshot
before submitting a login or another irreversible action. For the Codex plugin, the corresponding
actions are `type`, `type_physical`, and `type_unicode`.

Virtual-machine example:

```powershell
$tool = ".\desktopctl.exe"
$target = "VM_WINDOW_ID_OR_UNIQUE_NAME"
& $tool session start
try {
    & $tool window activate $target
    & $tool keyboard hotkey ctrl+a
    & $tool keyboard type-physical "admin@#"
    $capture = & $tool screenshot window $target | ConvertFrom-Json
    Write-Host "Verify the field in: $($capture.path)"
} finally {
    & $tool session stop
}
```

Activate the intended window immediately before keyboard input. Place the caret explicitly with
Ctrl+A before replacement, End or Ctrl+End before appending, and Home or Ctrl+Home before inserting
at the beginning. Verify the active window and resulting text or UI state afterward.

## Application discovery and launch

`app list` reads the current user's Windows AppsFolder and writes desktop and Microsoft Store
applications as UTF-8 JSON. `app open` prefers the stable ID from that list and also accepts a
unique exact display name. Use the ID when a name is ambiguous.

## Browser workflow

Keyboard commands target the foreground window. Do not manually enter a cross-window sequence
one line at a time because clicking the terminal changes focus. Paste the complete script or run
it from a file, and allow short waits after launch, window creation, and navigation.

```powershell
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
```

Replace the URL with `https://google.com` to navigate to Google. Use the selected window ID for
`window activate` and `screenshot window` when multiple Chrome windows exist.

## International paths and text

- Windows enters through `wmain`, accepts UTF-16 command arguments, uses native wide
  `std::filesystem::path` values and `W` Win32 APIs, and converts only the command and JSON
  boundary to UTF-8.
- Actual filesystem paths, application names, and window titles are preserved. Returned UTF-8
  strings may contain non-ASCII characters when the operating system data contains them.
- Ubuntu 22.04 uses UTF-8 command arguments and filesystem paths.
- Source filenames, identifiers, comments, help text, and fixed runtime messages use English
  ASCII text.

## Codex Desktop plugin

The `plugin` directory contains a portable Agent Plugins manifest, a local stdio MCP definition,
and a focused desktop-control skill. Protocol parsing and tool mapping live in a separate MCP
adapter layer; the existing input, capture, window, application, and indicator business classes do
not depend on MCP or plugin files.

Build, test, install, and package the unprotected Release executable with:

```powershell
cmake --build build-vs2019 --config Release --target desktopctl -- /m
ctest --test-dir build-vs2019 -C Release --output-on-failure
cmake --install build-vs2019 --config Release --prefix build-vs2019\install
.\scripts\packagePlugin.ps1
```

The package is written under `build-vs2019\package`. Extract
`desktopctl-plugin-0.9.6-windows-x64.zip`, open the desktop Plugins directory, select
`Add > Add a marketplace`, and choose the extracted package root as a local folder. Install
Desktop Control from the `Desktop Control Local` marketplace, restart the desktop app, and test
the plugin in a new local chat. The same package also supports the PowerShell installer workflow.
Its marketplace uses `./plugins/desktopctl`, and `mcp.json` launches
`${PLUGIN_ROOT}/bin/desktopctl.exe`; no development directory, fixed drive letter, or source-tree
path is embedded. The PowerShell package installer copies the marketplace to a current-user
location and uses `codex plugin marketplace add` followed by `codex plugin add`.

`desktopctl-plugin-0.9.6-upload.zip` is reserved for an approved local-MCP submission workflow.
Routine personal installation must use the local marketplace package because archive upload can
exclude local stdio MCP configuration when the submitting organization is not allowlisted.

## Exit codes

- `0`: success
- `2`: invalid command or arguments
- `3`: platform operation failure or operation not implemented on the current platform

## Windows limitations

- Input injection is constrained by Windows integrity levels and secure desktops. A normal
  process cannot reliably control an elevated window, the UAC secure desktop, or a locked screen.
- The indicator uses layered top-level windows. Screenshots taken while it is active include the
  green border and pointer halo.

## Architecture

- `CommandLineApplication` owns command parsing, validation, and result serialization.
- `McpServer` owns newline-delimited JSON-RPC transport and MCP lifecycle compatibility.
- `McpToolDispatcher` is a module-private adapter that validates MCP tool inputs, applies the
  mandatory session gate, and invokes the existing business contracts.
- `JsonCodec` and `JsonValue` are dependency-free module-private protocol components so the
  Windows runtime remains one statically linked EXE.
- `ApplicationCatalog` owns the cross-platform application catalog contract and isolates Windows
  Shell and COM details.
- `DesktopAutomation` owns the stable device-control contract and isolates platform code with
  PIMPL.
- `ScreenCapture` owns output paths, full desktop, region and target-window capture, window
  discovery, and screenshot metadata.
- `src/platform/windows` uses Win32 `SendInput`, `SetCursorPos`, `PrintWindow`, and GDI.
- `src/platform/linux` contains the Ubuntu 22.04 backend entry points for future X11 and Wayland
  adapters.
