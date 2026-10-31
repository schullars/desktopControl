#!/usr/bin/env bash

set -euo pipefail

rootfs="${1:-/opt/desktopctl-arm64-rootfs}"
mirror="${2:-http://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/}"
ready_marker="$rootfs/.desktopctl-aarch64-sysroot-ready"

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
    printf 'Run this script as root.\n' >&2
    exit 1
fi

case "$rootfs" in
    /|/opt|/usr|/home)
        printf 'Refusing unsafe sysroot path: %s\n' "$rootfs" >&2
        exit 1
        ;;
esac

for required_command in qemu-debootstrap qemu-aarch64-static chroot; do
    if ! command -v "$required_command" >/dev/null 2>&1; then
        printf 'Required command is missing: %s\n' "$required_command" >&2
        exit 1
    fi
done

if [[ -f "$ready_marker" ]]; then
    printf 'AArch64 sysroot is already ready: %s\n' "$rootfs"
    exit 0
fi

resume_existing=false
if [[ -e "$rootfs" ]] && find "$rootfs" -mindepth 1 -maxdepth 1 -print -quit \
    | grep -q .; then
    if [[ -x "$rootfs/bin/sh" && -f "$rootfs/etc/debian_version" ]]; then
        resume_existing=true
        printf 'Resuming completed base sysroot: %s\n' "$rootfs"
    else
        printf 'Refusing to overwrite a nonempty incomplete sysroot: %s\n' "$rootfs" >&2
        exit 1
    fi
fi

if [[ "$resume_existing" == false ]]; then
    printf 'Creating Ubuntu 22.04 aarch64 sysroot at %s\n' "$rootfs"
    qemu-debootstrap --arch=arm64 --variant=minbase jammy "$rootfs" "$mirror"
fi

install -m 0755 "$(command -v qemu-aarch64-static)" \
    "$rootfs/usr/bin/qemu-aarch64-static"

printf '%s\n' \
    "deb $mirror jammy main" \
    "deb $mirror jammy-updates main" \
    "deb $mirror jammy-security main" \
    > "$rootfs/etc/apt/sources.list"
cp --dereference /etc/resolv.conf "$rootfs/etc/resolv.conf"

chroot "$rootfs" /usr/bin/env DEBIAN_FRONTEND=noninteractive apt-get update
chroot "$rootfs" /usr/bin/env DEBIAN_FRONTEND=noninteractive apt-get install -y \
    libc6-dev libstdc++-11-dev pkg-config \
    libx11-dev libxext-dev libjpeg-dev libpng-dev libglib2.0-dev \
    libxtst6 libxrandr2

touch "$ready_marker"
printf 'AArch64 sysroot is ready: %s\n' "$rootfs"
