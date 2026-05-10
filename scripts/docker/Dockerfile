FROM debian:bookworm-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    bash \
    build-essential \
    bison \
    flex \
    ca-certificates \
    curl \
    grub-pc-bin \
    xorriso \
    qemu-system-x86 \
    make \
    gcc \
    g++ \
    binutils \
    git && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src

