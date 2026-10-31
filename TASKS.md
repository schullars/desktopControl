# DesktopCTL Task Queue

This file records planned work only. A task is not implemented, built, or verified until its status is updated with corresponding evidence.

## DESK-001 - Wayland overlay transparency and thin-border fallback

- Status: Completed
- Priority: Complete
- Scope:
  - Verify that the XWayland overlay uses an ARGB visual and that the compositor preserves per-pixel alpha.
  - Make the green gradient border visibly translucent on supported compositors.
  - Add a thinner-border fallback when reliable translucency is unavailable so text, icons, panels, and taskbars remain readable.
  - Preserve click-through behavior, topmost maintenance, multi-monitor alignment, and the pointer halo.
- Acceptance criteria:
  - The border does not visibly hide panel text or icons on Ubuntu 22.04 GNOME Wayland.
  - A screenshot confirms translucency or the thin fallback on every connected display.
  - Existing Windows and Xorg overlay behavior remains unchanged unless explicitly selected by the same fallback logic.
- Implementation evidence:
  - The Linux overlay now discovers a 32-bit ARGB visual through the XRender runtime interface and verifies it with an X11 probe window before use.
  - ARGB windows encode the green gradient with per-pixel alpha. Wayland sessions fall back to a three-pixel border when the probe fails.
  - The WSLg rootless X server exercised the failed-probe fallback without a session crash.
  - Ubuntu 22.04 GNOME Wayland used the three-pixel fallback on a 1600x900 VM display. The border remained visible without obscuring panel text or icons, and the pointer halo remained aligned with portal coordinates.
  - Reused portal authorization produced consecutive 1600x900 JPEG screenshots without a second prompt. The captured image was opened in the guest and visually verified as complete and non-black.
  - Ubuntu x86_64 and aarch64 builds pass with strict warnings. Both architectures pass 27 CTest cases, and the aarch64 QEMU X11 session and input smoke test passes.

## DESK-002 - Android library and RustDesk terminal integration

- Status: In progress
- Priority: Active
- Scope:
  - Add an independently buildable `desktopctl-android.aar` module with no RustDesk source dependency.
  - Keep command parsing, session state, the three-minute inactivity timeout, JSON responses,
    screenshot-path policy, application commands, and Android help text inside the AAR.
  - Define a narrow host interface for screen capture, accessibility input, application launch, and
    the visible green activity indicator.
  - Add a version-pinned RustDesk Android integration layer that adapts `MainService` and
    `InputService` to the AAR and exposes `desktopctl` through authenticated Terminal CLI requests.
  - Preserve the existing Windows and Ubuntu build targets and their command behavior.
- Acceptance criteria:
  - JVM unit tests cover help, session gating, activity refresh, timeout, command validation, JSON
    escaping, screenshot metadata, application list/open, pointer commands, and text input.
  - The Android library builds as one AAR and contains no RustDesk package references.
  - A customized RustDesk Android APK builds for arm64-v8a and x86_64 with the AAR embedded.
  - An Android emulator accepts Terminal CLI commands for session start, screenshot, pointer/touch,
    text input, application listing, application launch, timeout, and session stop.
  - Screenshot coordinates and injected input use the same current display dimensions and scale.
  - All new source, comments, help output, filenames, identifiers, and static messages use English
    ASCII text; runtime paths preserve the device's actual characters.
- Implementation evidence:
  - The isolated Android library builds with Android Gradle Plugin 8.7.3 and produces
    `desktopctl-android-release.aar`.
  - Eleven JVM unit tests pass for the command engine, session lifecycle, timeout behavior, JSON,
    screenshot metadata, application commands, pointer commands, hotkeys, and Unicode text.
  - The release AAR contains only `com.schullars.desktopctl` classes and has no RustDesk package
    reference.
  - Three Android instrumentation tests pass on an Android 14 x86_64 emulator with KVM
    acceleration. They cover real Android application discovery, JPEG and BMP encoding, session
    gating, and `@`/`#` text preservation.
  - The RustDesk adapter and a patch pinned to public commit
    `94c321d05ce02ee73304e6cf99187e2d4d96e409` are present under
    `android/rustdesk-integration`.
- Remaining verification:
  - Build and exercise the final customized RustDesk APK. This requires the exact Android source
    or APK revision that implements the installed Terminal CLI's private `remote-command-v1`
    transport; the public RustDesk source does not contain that protocol.

