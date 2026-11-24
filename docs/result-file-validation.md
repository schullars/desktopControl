# Generic result-file transport validation

## Scope and architecture

The source baseline is `9996c052ecacad822741d7cf531b022e8693e906`. That revision does not
implement `--result-file`; the historical PrintWindow note about a separately patched binary is
not evidence of an implementation in this branch. The user approved implementing the generic
capability and pushing the tested change directly to main on 2026-11-23.

`CommandLineApplication` remains the CLI owner. Its existing private implementation strips the
leading option and delegates single-command output capture/persistence to the module-private
`CommandResultFile` component. The latter reuses `JsonCodec`/`JsonValue` and never references
RustDesk, devices, session state, or another application class. This one-way, invocation-scoped
subordinate relationship leaves normal CLI and MCP dispatch untouched. The private component
does not need a recursively nested PIMPL; no new public API or namespace helper is introduced.

The result contract uses camelCase field names matching the consuming RustDesk schema version 1.
Existing destinations, including dangling symlinks, are rejected. An exclusive, non-inheritable
pending file is opened before the operation. Publishing is atomic and non-replacing. Error
envelopes preserve the actual command exit code; output-publication failures instead fail closed
with process exit code 3. No session, desktop privilege, UIAccess, or secure-desktop policy changes
are part of this patch.

## Validation record

### 2026-11-23 - Initial implementation

- Added the result-file component, CLI adapter, English help/reference, component tests, and CLI
  lifecycle tests. Both test targets are separate from the release executable.
- Confirmed GitHub main still points at the baseline before editing; local source was clean.
- Ubuntu build contract: Ubuntu 22.04, GCC 11.4, CMake 3.22.1, C++17, Release, strict warnings.
- Windows build-host decision is pending: documentation pins VS2019 16.11/v142/SDK19041, while
  the existing local tree uses VS2022 17.14 with the same v142 14.29 compiler and SDK19041.
  The user has been asked to approve that host-only override. No Windows build has been run
  under the new change before approval.
- Component, CLI, complete regression, signed UIAccess integration, and GitHub push are pending.
  This record is not a claim that implementation has passed acceptance.

### 2026-11-23 - Portable component verification

- Configuring the complete Ubuntu target stopped at the declared dependency check because
  `pkg-config` cannot find `libpipewire-0.3`. No dependency was installed or substituted.
- Built the actual module-private component and its test directly with the documented GCC 11.4
  compiler, `-std=c++17 -Wall -Wextra -Wpedantic -Werror`, using
  `test/commandResultFileTests.cpp`, `commandResultFile.cpp`, `jsonCodec.cpp`, and `jsonValue.cpp`.
  The first run passed. This is a component build, not a full desktop executable build.
- Added single-use guards and reran the complete component suite with AddressSanitizer and
  UndefinedBehaviorSanitizer (`-fsanitize=address,undefined -fno-omit-frame-pointer`): exit 0,
  `Result-file component tests passed.`, no sanitizer finding.
- The suite exercises exit codes 0/2/3, UTF-8/emoji filenames and output, embedded NUL/control
  characters, exact schema fields, stdout/stderr restoration, owner-only Linux mode, empty or
  invalid destinations, existing files/directories, dangling symlinks, abandoned invocation cleanup,
  publication collisions, repeated begin, and repeated finish.
- The modified CLI translation unit also passed GCC strict-warning `-fsyntax-only` verification.
  `git diff --check` passed. The full CLI CTest and real Windows UIAccess integration have not run.
- Still waiting for the Windows build-host decision. No GitHub commit or push has been made;
  the user's condition to publish only after complete verification remains in effect.

### 2026-11-23 - Windows host override approved

- The user approved continuing with the existing VS2022 host while retaining v142,
  Windows SDK 10.0.19041.0, x64, C++17, and the static runtime. This is a host-only override of
  the README's VS2019 requirement, not permission to upgrade compiler or SDK versions.
- Reverified cache source path, generator, instance, architecture, toolset, SDK, and test settings
  before reuse. Selected VS2022 Community 17.14.37827.9 and MSVC 14.29.30133/19.29.30159.
- The raw development build keeps UIAccess disabled; the separately staged installed executable
  must receive its existing UIAccess manifest and company signature before integration acceptance.
- The user confirmed autonomous regression, direct-main submission after passing tests, signed
  protected/unprotected Windows installers, and later cross-platform synchronization. No sharing
  or production publication is authorized by this continuation.

### 2026-11-23 - Windows regression and signed installed integration progress

- CMake/MSBuild Release x64 build passed on the approved VS2022/v142/SDK19041 profile.
  MSBuild was 17.14.60. The raw executable is 631808 bytes, SHA-256
  `103FBE9470DB624BF2B764FA8A21BA21FEB15EA975FE8D0F2737B4B3BD672429`.
- All 31 registered Windows test cases passed: 28 noninteractive CTest cases on the development
  host and the remaining MCP lifecycle, activity-indicator Z-order, and session lifecycle cases
  in the isolated RDQA-W10 guest with the exact same executable. The owned session stopped.
- Ubuntu component sanitizer tests remain passed; a complete Ubuntu build still requires its
  declared PipeWire development dependency. No full Linux result is inferred from component tests.
- Staged the existing asInvoker/UIAccess manifest and company OV signature, with timestamp.
  Signed DesktopCTL SHA-256 is
  `BA3777572A0C98AA3FE1FB4CCE6CA5D8B61438AB7CDF79F994BD6D8D6ECEE42C`.
- A new RustDesk build149 candidate reuses 98 unchanged previously verified payload files and
  changes only DesktopCTL. Its actual signed installer completed a normal-UAC QA upgrade;
  all 99 installed payload hashes matched, service Running/Auto/LocalSystem, no manual replacement.
- Actual Linux Terminal CLI to the installed Windows service completed 64 old148/new direct/relay
  connection cases. Result-file help, session start, window list, activation, two downloaded window
  screenshots, physical text with mixed case and `@#`, and button input returned successful results.
- Integration acceptance is still incomplete: the first UI fixture was hidden by the QA launcher's
  STARTUPINFO SW_HIDE. The QA-only fixture now records native visibility and re-shows only its own
  window. The second run visibly showed the exact input and one click, but its 30-minute GuestOps
  credential expired during final observation. Its cleanup-required report is retained; recovery
  and a complete fresh run are required before the integration gate or GitHub submission passes.
- QA runtime packaging failures (argument-array handling, then missing CMAKE_ROOT) are preserved
  separately. The final successful run used the existing complete Kitware CMake 3.26.4 runtime,
  not a fabricated module marker or a system-wide installation. Product behavior was not changed
  to accommodate those test-runner failures.

### 2026-11-23 - Independent recovery and remaining full-run gate

- The expired-credential run was recovered using a fresh, scoped GuestOps credential. An
  independent observation confirmed exact text `QA149_AbC@#_10f7e5bd` and click count 1.
  Original profiles and the automatic LocalSystem service were restored and the temporary
  firewall rule removed. The original cleanup-required summary was not rewritten as a pass.
- A complete fresh run was attempted, but host foreground changes to Explorer and WeChat
  interfered with normal QA UAC confirmation. Its ready deadline expired before any profile
  transition report or installed-input report appeared. A separate audit found no active lease,
  no user-profile ownership marker, and the original Running/Auto/LocalSystem service intact.
- Input was not sent when the host focus guard identified another application. A screenshot
  occluded by WeChat was rejected as evidence. The user was asked to pause competing input;
  no UAC policy or foreground-security restriction was bypassed.
- The second temporary QA credential was positively restored to a blank password at
  `2026-11-24T00:41:06.6860081Z` and removed from the host. The owned bootstrap and desktop bridge
  stopped, and the QA VM shut down; VMware/VMX process and running-VM counts were zero.
- The final uninterrupted UI/SCM-restart run, remaining product gates, and GitHub push are still
  pending. The source, failed runs, successful component/CLI tests, installed candidate, and
  recovery receipts remain local. No protected release or production publication is claimed.

### 2026-11-23 - Source submission scope

The user requested submitting tested source before continuing the remaining system tests. The
independent DesktopCTL change now has all 31 Windows regression cases plus real signed installed
result-file, screenshot, elevated keyboard/mouse, and independently confirmed exact-text/one-click
evidence. That supports a scoped source submission; it does not close RustDesk's uninterrupted
SCM-restart or release acceptance gate. Full Ubuntu builds/CI and the remaining system matrix must
continue afterwards. The earlier combined commit/release hold is therefore split into a tested
DesktopCTL source gate and the still-open RustDesk delivery gate; no failed run is relabeled.
