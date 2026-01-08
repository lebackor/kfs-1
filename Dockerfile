FROM ubuntu:22.04

# Paquet necessaire pour generer l'ISO (make pour makefile, xorriso pour generer l'iso, grub bootloader integrer dans l'iso)
RUN apt-get update && apt-get install -y --no-install-recommends \
    make xorriso mtools grub-pc-bin grub-common \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]
