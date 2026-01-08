FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Toolchain and utilities needed to build the kernel and ISO
RUN apt-get update && apt-get install -y --no-install-recommends \
    make xorriso mtools grub-pc-bin grub-common \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]
