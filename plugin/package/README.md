# Desktop Control Plugin for Codex Desktop

Desktop Control is a Windows desktop-only plugin. It provides structured local tools for application
discovery and launch, window discovery and activation, mouse and keyboard input, and screenshots.
Every desktop operation requires a visible green activity session.

## Requirements

- Windows 10 or Windows 11 on an interactive, unlocked desktop.
- ChatGPT Desktop or Codex Desktop with plugin support enabled.
- A local Codex chat. Cloud-only chats cannot run the bundled executable.

The runtime is a single statically linked `desktopctl.exe`. It does not require the Visual C++ runtime
or an external JSON library. Windows system DLLs are supplied by the operating system.

## Install through the desktop user interface

Use `desktopctl-plugin-0.9.1-upload.zip` for this installation method. Do not extract it.

1. Open ChatGPT Desktop and switch to the ChatGPT surface if the app is showing Codex.
2. Open the Plugins directory.
3. Select `Add`, then `Upload plugin archive`.
4. Select `desktopctl-plugin-0.9.1-upload.zip` and choose `Add plugin`.
5. On the Desktop Control details page, choose `Install plugin`.
6. Wait until Desktop Control appears under `Installed` and the page shows `Manage` and `Try now`.
7. Open a new local chat before testing the plugin.

Uploading the archive and installing the parsed plugin are separate user-interface steps. Seeing the
plugin details page alone does not mean installation is complete.

## Install through PowerShell

Use `desktopctl-plugin-0.9.1-windows-x64.zip` for this alternative method. It requires the `codex`
command to be available on `PATH`.

1. Extract the ZIP to any local directory. Preserve the directory structure.
2. Open PowerShell in the extracted directory.
3. Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\Install.ps1
```

The installer copies the marketplace to the current user's local application-data directory,
registers the `desktopctl-local` marketplace, and installs `desktopctl@desktopctl-local`. Restart
Codex Desktop afterward, then open a new local chat and select Desktop Control.

Neither archive contains a development-machine path or fixed drive letter. The marketplace points
to `./plugins/desktopctl`, and the MCP configuration resolves the executable from `${PLUGIN_ROOT}`.
Therefore the PowerShell package may be extracted anywhere, and the installer may use another
destination:

```powershell
powershell -ExecutionPolicy Bypass -File .\Install.ps1 -InstallRoot "D:\PortableApps\DesktopCtlPlugin"
```

## Test after installation

Start a new local chat and ask:

```text
Use Desktop Control to start a session, list the available applications, and then stop the session.
```

For a screenshot workflow, ask:

```text
Use Desktop Control to start a session, list windows, activate Notepad if it is present, capture that
window as a JPG, report the returned path and image count, and stop the session.
```

The green border and pointer halo must appear after `session_start`. Every other desktop tool is
rejected until that tool succeeds. The indicator closes after three minutes without a tool call or
immediately after `session_stop`.

## Relative paths and user data

All package-internal paths are relative to the marketplace or plugin root. Screenshot results return
absolute runtime paths because callers need to open the generated files and because actual Windows
paths may contain non-ASCII characters. Omitting the screenshot output path creates a quality-95 JPG
inside the stable `%TEMP%\DesktopCTL\screenshots` directory for the current user. This remains
writable when the plugin or executable is installed under a protected system directory.

## Uninstall

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\Uninstall.ps1
```

The script unregisters the plugin and marketplace but leaves copied files in place for recovery. It
prints the directory that may be removed manually after Codex Desktop is closed.

## Limitations

The current implementation supports Windows. Ubuntu 22.04 entry points remain reserved in the C++
architecture but are not implemented. A normal process cannot reliably control elevated windows, the
UAC secure desktop, or a locked screen.
