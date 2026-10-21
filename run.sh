#!/usr/bin/env bash

# run.sh only purpose: testing

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARCH="${ARCH:-x86_64}"
ARCH_UPPER="$(echo "$ARCH" | tr '[:lower:]' '[:upper:]')"
EFI_IMAGE="$ROOT/build/BOOT${ARCH_UPPER}.EFI"

case "$ARCH" in
    x86_64)
        QEMU="qemu-system-x86_64"
        FALLBACK_NAME="BOOTX64.EFI"
        QEMU_ARGS=(-machine q35 -cpu qemu64)
        FIRMWARE_CANDIDATES=(
            /usr/share/OVMF/OVMF_CODE.fd
            /usr/share/OVMF/OVMF_CODE_4M.fd
            /usr/share/edk2/x64/OVMF_CODE.4m.fd
            /usr/share/edk2-ovmf/x64/OVMF_CODE.fd
        )
        ;;
    aarch64)
        QEMU="qemu-system-aarch64"
        FALLBACK_NAME="BOOTAA64.EFI"
        QEMU_ARGS=(-machine virt -cpu cortex-a72)
        FIRMWARE_CANDIDATES=(
            /usr/share/AAVMF/AAVMF_CODE.fd
            /usr/share/edk2/aarch64/QEMU_EFI.fd
            /usr/share/qemu-efi-aarch64/QEMU_EFI.fd
        )
        ;;
    *)
        echo "unsupported ARCH: $ARCH (use x86_64 or aarch64)" >&2
        exit 1
        ;;
esac

if [[ ! -f "$EFI_IMAGE" ]]; then
    echo "missing $EFI_IMAGE, run: make ARCH=$ARCH" >&2
    exit 1
fi

FIRMWARE="${OVMF:-}"
if [[ -z "$FIRMWARE" ]]; then
    for candidate in "${FIRMWARE_CANDIDATES[@]}"; do
        if [[ -f "$candidate" ]]; then
            FIRMWARE="$candidate"
            break
        fi
    done
fi
if [[ -z "$FIRMWARE" || ! -f "$FIRMWARE" ]]; then
    echo "no UEFI firmware found, install OVMF/edk2 or set OVMF=/path/to/firmware" >&2
    exit 1
fi

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/EFI/BOOT"
cp "$EFI_IMAGE" "$STAGE/EFI/BOOT/$FALLBACK_NAME"

"$QEMU" \
    "${QEMU_ARGS[@]}" \
    -m 256M \
    -bios "$FIRMWARE" \
    -drive format=raw,file="fat:rw:$STAGE" \
    -nic none \
    -serial stdio \
    -no-reboot
