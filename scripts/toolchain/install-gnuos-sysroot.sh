#!/usr/bin/env bash
set -euo pipefail

ARCH="${ARCH:-x86_64}"
TARGET="${TARGET:-${ARCH}-gnuos}"
BUILD_DIR="${BUILD_DIR:-build/${ARCH}}"
SYSROOT="${SYSROOT:-${BUILD_DIR}/sysroot/${TARGET}}"
HEADERS_DIR="${HEADERS_DIR:-userspace/libc/include}"
STARTFILES_DIR="${STARTFILES_DIR:-${BUILD_DIR}/userspace/libc/startfiles/${ARCH}}"

echo "[sysroot] arch=$ARCH"
echo "[sysroot] target=$TARGET"
echo "[sysroot] build_dir=$BUILD_DIR"
echo "[sysroot] sysroot=$SYSROOT"

for obj in crt0.o crti.o crtn.o; do
    if [[ ! -f "${STARTFILES_DIR}/${obj}" ]]; then
        echo "[sysroot] missing startfile: ${STARTFILES_DIR}/${obj}" >&2
        exit 1
    fi
done

if [[ ! -d "$HEADERS_DIR" ]]; then
    echo "[sysroot] missing headers dir: $HEADERS_DIR" >&2
    exit 1
fi

mkdir -p "${SYSROOT}/usr/lib" "${SYSROOT}/usr/include"

install -m 0644 "${STARTFILES_DIR}/crt0.o" "${SYSROOT}/usr/lib/crt0.o"
install -m 0644 "${STARTFILES_DIR}/crti.o" "${SYSROOT}/usr/lib/crti.o"
install -m 0644 "${STARTFILES_DIR}/crtn.o" "${SYSROOT}/usr/lib/crtn.o"

rm -rf "${SYSROOT}/usr/include"
mkdir -p "${SYSROOT}/usr/include"
cp -R "${HEADERS_DIR}/." "${SYSROOT}/usr/include/"

echo "[sysroot] installed startfiles and headers."
