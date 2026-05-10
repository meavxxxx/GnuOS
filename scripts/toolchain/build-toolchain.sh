#!/usr/bin/env bash
set -euo pipefail

TARGET="${TARGET:-x86_64-elf}"
PREFIX="${PREFIX:-$HOME/.local/cross/$TARGET}"
BINUTILS_VERSION="${BINUTILS_VERSION:-2.43}"
GCC_VERSION="${GCC_VERSION:-14.2.0}"
JOBS="${JOBS:-$(nproc)}"
WORKDIR="${WORKDIR:-$PWD/build/toolchain-src}"

echo "[toolchain] target=$TARGET"
echo "[toolchain] prefix=$PREFIX"
echo "[toolchain] workdir=$WORKDIR"

mkdir -p "$WORKDIR" "$PREFIX"
cd "$WORKDIR"

if [[ ! -f "binutils-$BINUTILS_VERSION.tar.xz" ]]; then
    curl -LO "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz"
fi

if [[ ! -f "gcc-$GCC_VERSION.tar.xz" ]]; then
    curl -LO "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz"
fi

if [[ ! -d "binutils-$BINUTILS_VERSION" ]]; then
    tar -xf "binutils-$BINUTILS_VERSION.tar.xz"
fi

if [[ ! -d "gcc-$GCC_VERSION" ]]; then
    tar -xf "gcc-$GCC_VERSION.tar.xz"
fi

mkdir -p build-binutils
pushd build-binutils >/dev/null
"../binutils-$BINUTILS_VERSION/configure" \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror
make -j"$JOBS"
make install
popd >/dev/null

mkdir -p build-gcc
pushd build-gcc >/dev/null
"../gcc-$GCC_VERSION/configure" \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c,c++ \
    --without-headers
make -j"$JOBS" all-gcc
make -j"$JOBS" all-target-libgcc
make install-gcc
make install-target-libgcc
popd >/dev/null

echo "[toolchain] done"
echo "[toolchain] add to PATH: export PATH=\"$PREFIX/bin:\$PATH\""

