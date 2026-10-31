# DesktopCTL

DesktopCTL is a C++17 command-line desktop automation tool for Windows and Ubuntu 22.04. It
controls pointer and keyboard input, discovers and launches applications, inspects and activates
windows where the display protocol permits it, captures screenshots, and displays a visible green
per-monitor activity border and pointer halo while automation is active.

The Windows executable remains a single statically linked EXE. The Linux backend uses the native
desktop libraries supplied by Ubuntu. Platform code is isolated under `src/platform/windows` and
`src/platform/linux`; command parsing, MCP transport, JSON results, and public business contracts
are shared.

## Platform capability matrix

| Feature | Windows 10/11 | Ubuntu Xorg/X11 | Ubuntu Wayland |
| --- | --- | --- | --- |
| Green multi-monitor border and pointer halo | Yes | Yes | Yes, through XWayland |
| Absolute/relative pointer and buttons | Yes | Yes | Yes, after portal approval |
| Hotkeys and named keys | Yes | Yes | Yes, after portal approval |
| Hybrid/Unicode text | Yes | Yes | Yes |
| Layout-backed physical text | Yes | Yes | No standard portal support; use Xorg |
| Full desktop screenshot | Yes | Yes | Yes, through Screenshot portal |
| Coordinate region screenshot | Yes | Yes | No standard compositor-independent API |
| Global window list and bounds | Yes | Yes | No standard compositor-independent API |
| Forced window activation | Yes | Yes | No standard compositor-independent API |
| Programmatic target-window capture | Yes | Yes | No standard compositor-independent API |
| Application catalog and launch | AppsFolder | GIO desktop catalog | GIO desktop catalog |

Wayland deliberately restricts global desktop access. DesktopCTL uses XDG Desktop Portal
RemoteDesktop for pointer and keyboard input and the Screenshot portal for full-screen capture.
This follows the common portal path used by GNOME Shell and KDE Plasma rather than depending on a
compositor-specific private API. The compositor can show an authorization dialog when `session
start` is called. Unsupported Wayland operations return explicit errors instead of silently
claiming success. The required green indicator uses XWayland because standard Wayland has no
unrestricted global overlay-placement API; session start fails if the indicator cannot be
displayed.

## Build

### Windows with Visual Studio 2019

Use Visual Studio 2019 16.11, the v142 toolset, and Windows SDK 10.0.19041:

```powershell
cmake -S . -B build-vs2019 -G "Visual Studio 16 2019" -A x64 `
  -DCMAKE_SYSTEM_VERSION=10.0.19041.0
cmake --build build-vs2019 --config Release --target desktopctl -- /m
ctest --test-dir build-vs2019 -C Release --output-on-failure
cmake --install build-vs2019 --config Release --prefix build-vs2019\install
```

Release uses the static MSVC runtime (`/MT`). `desktopctl.exe` therefore does not require a
separate Visual C++ redistributable or external JSON library.

### Ubuntu 22.04 x86_64 or aarch64

Install the same architecture's development packages:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config \
  libx11-dev libxext-dev libjpeg-dev libpng-dev libglib2.0-dev \
  libxtst6 libxrandr2
```

Build natively:

```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --parallel
ctest --test-dir build-linux --output-on-failure
cmake --install build-linux --prefix build-linux/install
```

The Linux source contains no x86-specific instructions or pointer-size assumptions. The same
commands work on Ubuntu 22.04 aarch64 when the packages above are installed for aarch64. Native
building is preferred; a cross build may instead provide an aarch64 toolchain file and matching
sysroot packages.

Linux dynamically loads XTest and XRandR runtime entry points so the public backend stays portable
across x86_64 and aarch64. GIO, X11/Xext, JPEG, and PNG are normal dynamic Ubuntu dependencies.
Wayland sessions also require XWayland, `xdg-desktop-portal`, and the desktop environment's portal
backend, such as the GNOME or KDE implementation. Standard Ubuntu Desktop and Kubuntu installations
normally provide these components.

## Command summary

Run `desktopctl -H` for the complete reference, platform notes, JSON schemas, and examples.

```text
desktopctl session start
desktopctl session stop

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

desktopctl screenshot
desktopctl screenshot full [OUTPUT.jpg]
desktopctl screenshot region X Y WIDTH HEIGHT [OUTPUT.jpg]
desktopctl screenshot window "ID_OR_NAME" [OUTPUT.jpg]

desktopctl window list
desktopctl window activate "ID_OR_NAME"

desktopctl app list
desktopctl app open ID_OR_NAME

desktopctl mcp serve
```

## Mandatory session lifecycle

Help (`-H`, `-h`, `--help`, or `help`) is always available. Every other desktop command is
rejected with exit code `3` until `desktopctl session start` succeeds. The start command waits
until the green indicator and the required platform input session are ready. On Wayland this can
include a user-consent dialog.

Every accepted non-help command refreshes a three-minute inactivity deadline. After three minutes
without activity, the detached host closes the overlay, releases the portal session, and clears
state. `session stop` performs the same cleanup immediately. Windows owns state with named kernel
objects. Linux uses a mode-0700 per-user runtime directory and a mode-0600 Unix socket under
`$XDG_RUNTIME_DIR/desktopctl`, falling back to `/tmp/desktopctl-UID` when required.

PowerShell:

```powershell
$tool = ".\desktopctl.exe"
& $tool session start
if ($LASTEXITCODE -ne 0) { throw "Session start failed" }
try {
    & $tool app list
    & $tool screenshot
} finally {
    & $tool session stop
}
```

Bash:

```bash
tool=./desktopctl
"$tool" session start || exit 1
trap '"$tool" session stop >/dev/null 2>&1 || true' EXIT
"$tool" app list
"$tool" screenshot
```

## Input and focus

Hotkeys accept separate keys or a plus expression. These are equivalent:

```text
desktopctl keyboard hotkey ctrl shift escape
desktopctl keyboard hotkey ctrl+shift+escape
```

`keyboard type` is recommended for normal UTF-8 text. It uses layout-backed key chords when
available and falls back to the platform Unicode mechanism. `type-physical` rejects characters
that cannot be represented by physical keys; this is useful for ASCII passwords and symbols sent
through a virtual-machine or remote-desktop client. `type-unicode` forces Unicode delivery.

With a US layout, `@` is Shift+2 and `#` is Shift+3. DesktopCTL sends those chords when the literal
characters appear in `type` or `type-physical`; callers should not manually send the modifiers as
well. The host and guest keyboard layouts, input methods, and lock-key states still need to agree.
Command success confirms that the platform accepted input, not that the target rendered it. Always
activate the intended window, place the caret, type the whole value, and verify the screenshot
before submitting credentials or another irreversible action.

On Wayland, `type-physical` is unavailable because the standard portal sends keysyms and does not
expose target-layout scan-code discovery. Use `type`, `type-unicode`, or an Xorg login session.

## Screenshots and cleanup metadata

Omitting the output path creates a timestamped quality-95 JPG under the current user's temporary
directory:

- Windows: `%TEMP%\DesktopCTL\screenshots`
- Linux: `${TMPDIR:-/tmp}/DesktopCTL/screenshots`

The location is independent of the executable and remains writable when the tool is installed in a
protected system directory. `.jpg` and `.jpeg` use JPEG quality 95. `.bmp` remains available when
lossless uncompressed pixels are explicitly required. Use JPG for routine work.

Every successful capture writes one UTF-8 JSON object:

```json
{
  "success": true,
  "path": "/tmp/DesktopCTL/screenshots/screenshot_20261030_120000_123.jpg",
  "directory": "/tmp/DesktopCTL/screenshots",
  "imageCount": 51,
  "cleanupRecommended": true,
  "message": "Screenshot saved. The output directory contains 51 image file(s). More than 50 images are present; remove screenshots you no longer need."
}
```

Returned paths are absolute UTF-8 strings and preserve actual filesystem characters. The count
includes JPG, JPEG, and BMP files in the selected directory. DesktopCTL only recommends cleanup;
it never deletes screenshots automatically.

## Application and window workflow

`app list` returns stable application IDs and display names. `app open` accepts a returned ID or a
unique exact display name. Launching is asynchronous, and an application may reuse an existing
window.

On Windows or Xorg, use `window list`, select the transient ID, call `window activate`, then send
input. Window IDs remain valid only while the window exists. On Wayland, global window discovery,
forced activation, coordinate regions, and target-window capture return a clear unsupported error.
Use application launch, portal-authorized input, and a full screenshot instead.

## Plugin packaging

The plugin business adapter remains separate from desktop automation code. All package references
are relative to `${PLUGIN_ROOT}`.

Windows package:

```powershell
.\scripts\packagePlugin.ps1
```

Linux package after installing to `build-linux/install`:

```bash
./scripts/packagePluginLinux.sh
```

The Windows package embeds `bin/desktopctl.exe`; the Linux package embeds `bin/desktopctl` and
stages Linux-specific relative MCP configurations. Do not use the protected or packed Windows EXE
for the plugin package.

## Exit codes and limitations

- `0`: success
- `2`: invalid command or arguments
- `3`: missing session, platform failure, denied authorization, or unsupported display operation

The tool controls only the current unlocked interactive desktop. Windows integrity levels, UAC,
secure desktops, Wayland compositor policy, sandboxing, remote clients, and application input
methods can restrict or reinterpret operations. The indicator and pointer halo appear in ordinary
screen captures by design so users can see that automation is active.

## Architecture

- `CommandLineApplication` owns command parsing, validation, help, and result serialization.
- `McpServer` and `McpToolDispatcher` adapt shared business contracts to local JSON-RPC tools.
- `ApplicationCatalog` owns application discovery and launch.
- `DesktopAutomation` owns pointer and keyboard control.
- `ScreenCapture` owns screenshots, window discovery, and activation.
- `ActivityIndicator` owns the visible indicator, session gate, IPC, and idle timeout.
- `src/platform/windows` isolates Win32, GDI, Shell, COM, and `SendInput` details.
- `src/platform/linux` isolates X11/XTest, GIO, XDG Desktop Portal, image codecs, and Unix IPC.
