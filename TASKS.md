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

