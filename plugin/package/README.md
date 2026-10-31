# Desktop Control Plugin

Desktop Control exposes the bundled DesktopCTL runtime as local structured tools for application
launch, window operations, pointer and keyboard input, and screenshots. Every operation requires a
visible green activity session.

## Choose the correct package

- Windows x64: `desktopctl-plugin-0.10.0-windows-x64.zip`
- Ubuntu x86_64: `desktopctl-plugin-0.10.0-linux-x86_64.zip`
- Ubuntu aarch64: `desktopctl-plugin-0.10.0-linux-aarch64.zip`

Do not copy only the executable. Keep `.agents`, `plugins`, and the package files together. All
runtime paths are relative to the plugin root, so the extracted directory can be moved.

## Install through the desktop user interface

1. Extract the ZIP to a local folder.
2. Open the desktop app and its Plugins directory.
3. Select `Add`, then `Add a marketplace`.
4. Enter the extracted package root as the local source. Leave Git ref and sparse paths empty.
5. Open `Desktop Control Local` and install Desktop Control.
6. Restart the desktop app and test from a new local chat.

Do not use `Upload plugin archive` for routine personal installation. Organization policy can
exclude local stdio MCP servers from that submission route. The local marketplace package includes
both the portable manifest and explicit Codex compatibility manifest.

The Windows package also includes `Install.ps1` and `Uninstall.ps1` for the Codex CLI marketplace
workflow. Run them from PowerShell in the extracted directory when that workflow is preferred.

## Test after installation

Ask:

```text
Use Desktop Control to start a session, list available applications, take a full screenshot, report
the returned path and image count, and stop the session.
```

The green border and pointer halo must appear after `session_start`. The indicator closes after
three minutes without a tool call or immediately after `session_stop`.

For explicitly unattended work, call `session_start` with `keep_alive: true`. This disables only
the inactivity timeout; `session_stop`, logout, reboot, host failure, or a required portal failure
still ends the session.

On Ubuntu Wayland, session start can show one desktop-portal approval dialog. It times out after 60
seconds if it is not confirmed. Full screenshots reuse the authorized ScreenCast session without a
new approval for every image. Global window listing, forced window activation, coordinate
region capture, programmatic target-window capture, and physical scan-code text are not standard
Wayland capabilities. Use Xorg when those operations are required. The green indicator requires
XWayland.

## Screenshot location

Omitting the output path creates a quality-95 JPG in the current user's stable temporary folder:

- Windows: `%TEMP%\DesktopCTL\screenshots`
- Linux: `${TMPDIR:-/tmp}/DesktopCTL/screenshots`

The result contains the absolute path, directory, image count, cleanup recommendation, and English
status message. The tool recommends cleanup after more than 50 images but never deletes them
automatically.

## Security and limitations

The plugin runs only in a local execution environment and controls the current unlocked interactive
desktop. Windows integrity levels, secure desktops, Wayland compositor policy, remote clients, and
application input methods can restrict or reinterpret operations. Verify the active target and
visible result before submitting credentials or another irreversible action.
