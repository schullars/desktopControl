#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 2 ]]; then
    printf 'Usage: %s DESKTOPCTL_EXECUTABLE AARCH64_SYSROOT\n' "$0" >&2
    exit 2
fi

executable_path="$1"
sysroot="$2"

if [[ ! -x "$executable_path" ]]; then
    printf 'Executable is missing or not executable: %s\n' "$executable_path" >&2
    exit 1
fi
if [[ ! -d "$sysroot" ]]; then
    printf 'AArch64 sysroot is missing: %s\n' "$sysroot" >&2
    exit 1
fi
if ! command -v qemu-aarch64-static >/dev/null 2>&1; then
    printf 'qemu-aarch64-static is not installed.\n' >&2
    exit 1
fi
if [[ -z "${DISPLAY:-}" ]]; then
    printf 'DISPLAY is not set. An X11 server is required.\n' >&2
    exit 1
fi

run_arm() {
    env -u WAYLAND_DISPLAY XDG_SESSION_TYPE=x11 QEMU_LD_PREFIX="$sysroot" \
        qemu-aarch64-static -L "$sysroot" "$executable_path" "$@"
}

stop_session() {
    run_arm session stop >/dev/null 2>&1 || true
}
trap stop_session EXIT

stop_session
run_arm session start
run_arm mouse move-relative 3 2
run_arm keyboard press shift

run_arm session stop
trap - EXIT
printf 'AArch64 X11 smoke test passed.\n'
