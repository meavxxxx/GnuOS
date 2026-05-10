FROM debian:bookworm-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    bash \
    build-essential \
    bison \
    flex \
    ca-certificates \
    curl \
    grub-common \
    grub2-common \
    grub-pc-bin \
    mtools \
    xorriso \
    qemu-system-x86 \
    make \
    gcc \
    g++ \
    binutils \
    git && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
