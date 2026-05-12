#!/usr/bin/env bash
set -euo pipefail

ARCH="${ARCH:-x86_64}"
EFI_FILE="${1:-build/${ARCH}/boot/efi/${ARCH}/BOOTX64.EFI}"
QEMU_SYSTEM="${QEMU_SYSTEM:-qemu-system-x86_64}"

find_ovmf_code() {
    local candidates=(
        "/usr/share/qemu/OVMF.fd"
        "/usr/share/ovmf/OVMF.fd"
        "/usr/share/OVMF/OVMF_CODE.fd"
        "/usr/share/OVMF/OVMF_CODE_4M.fd"
    )
    local path
    for path in "${candidates[@]}"; do
        if [[ -f "$path" ]]; then
            echo "$path"
            return 0
        fi
    done
    return 1
}

if [[ ! -f "$EFI_FILE" ]]; then
    echo "UEFI binary not found: $EFI_FILE"
    echo "Run: make ARCH=$ARCH uefi-stub"
    exit 1
fi

OVMF_CODE="$(find_ovmf_code || true)"
if [[ -z "${OVMF_CODE:-}" ]]; then
    echo "OVMF firmware not found."
    echo "Install package: ovmf"
    exit 1
fi

EFI_ROOT="$(mktemp -d)"
trap 'rm -rf "$EFI_ROOT"' EXIT
mkdir -p "$EFI_ROOT/EFI/BOOT"
cp "$EFI_FILE" "$EFI_ROOT/EFI/BOOT/BOOTX64.EFI"

"$QEMU_SYSTEM" \
    -bios "$OVMF_CODE" \
    -drive format=raw,file=fat:rw:"$EFI_ROOT"
