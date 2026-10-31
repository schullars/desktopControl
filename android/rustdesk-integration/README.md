# RustDesk Android integration

This directory contains the source adapter that embeds `desktopctl-android.aar`
inside a RustDesk Android application. The AAR has no compile-time dependency on
RustDesk. Only this adapter imports RustDesk classes and protocol types.

## Required host contract

The RustDesk build must provide all of the following:

- a `MainService` that owns the active MediaProjection `ImageReader`;
- an enabled `InputService` accessibility service;
- the RustDesk `hbb.MessageOuterClass` Java protocol classes;
- a remote-command transport compatible with the controlling Terminal CLI.

The public RustDesk source currently disables the terminal service on Android.
The locally installed Terminal CLI expects the private `remote-command-v1`
transport for non-Windows peers. An official RustDesk APK does not satisfy that
last requirement. Apply this adapter only to the exact RustDesk source revision
used to build the target APK or to a source tree containing the matching remote
command protocol.

## Source integration

1. Copy `desktopctl-android.aar` to `flutter/android/app/libs/`.
2. Add this dependency to `flutter/android/app/build.gradle`:

   ```groovy
   implementation files('libs/desktopctl-android.aar')
   ```

3. Copy the Kotlin sources below `app/src/main/kotlin/` into the matching
   RustDesk application source tree.
4. Create one `DesktopCtlRustDeskBridge` in `MainService.onCreate()` and close it
   in `MainService.onDestroy()`.
5. In the `ImageReader` callback, call `bridge.onImage(image)` before the image
   is closed. The bridge copies a frame only while a screenshot request waits.
6. Expose a kept `MainService` method that forwards one virtual command to
   `bridge.execute(commandLine)`.
7. Route only remote command scripts whose first token is `desktopctl` to that
   method. Reject every other Android script. Return the bridge JSON fields as
   the remote command exit code, stdout, and stderr.

`patches/rustdesk-94c321d-main-service.patch` contains the application-side
changes against public RustDesk commit
`94c321d05ce02ee73304e6cf99187e2d4d96e409`. It intentionally does not patch a
remote-command transport because that protocol is absent from the public source.
The matching transport patch must come from the same source revision as the
controlling Terminal CLI.

Do not extract an executable into app storage and invoke it with
`/system/bin/sh`. Modern Android blocks that design, and it would unnecessarily
expose a general command-execution path. The AAR executes inside the RustDesk app
process and reuses the already authorized capture and accessibility services.

## Runtime behavior

- Help works before a session starts.
- Every non-help command requires `desktopctl session start`.
- The green accessibility overlay is click-through and disappears on stop,
  bridge close, or the standard three-minute inactivity timeout.
- Screenshot coordinates and absolute pointer coordinates share the RustDesk
  capture coordinate space. The adapter applies `SCREEN_INFO.scale` only when
  injecting the physical gesture or drawing the pointer indicator.
- Screenshots use the RustDesk app cache directory:
  `cacheDir/DesktopCTL/screenshots`.
- Unicode text such as `@` and `#` uses RustDesk's text sequence path. Physical
  scan-code text input is explicitly unsupported on stock Android.
