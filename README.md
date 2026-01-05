# KFS-1 (Kernel From Scratch - 1)

A minimal 32-bit x86 kernel written in C and Assembly, developed as a foundational project to understand operating system development.

![License](https://img.shields.io/badge/license-MIT-blue.svg)

## 📖 Overview

KFS-1 is a basic kernel that boots via Multiboot and provides a functional shell-like environment. It interacts directly with hardware components like the VGA text buffer and keyboard controller (PS/2) without relying on standard libraries.

## ✨ Features

- **Multiboot Compliant**: Boots using GRUB or any Multiboot-compliant bootloader.
- **VGA Text Mode Driver**:
  - Full support for 80x25 text mode.
  - 16-color support (foreground and background).
  - Software scrolling handling.
  - `printf` implementation (`%s`, `%c`, `%d`, `%x`).
- **Input Handling**:
  - PS/2 Keyboard polling driver.
  - Support for typing, backspace (with prompt protection), and navigation (Arrow Keys).
- **Virtual Terminals**:
  - Support for 3 simultaneous screens.
  - Switch between screens using `F1`, `F2`, and `F3`.

## 🛠️ Build Requirements

To build and run this project, you need a Linux environment with the following tools:

- **GCC** (GNU Compiler Collection) configured for i386 (or a cross-compiler like `i686-elf-gcc`)
- **Binutils** (`as`, `ld`)
- **QEMU** (for emulation)
- **Xorriso** and **GRUB** (for creating bootable ISOs)

On a Debian/Ubuntu based system, you can install dependencies via:
```bash
sudo apt update
sudo apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo qemu-system-x86 xorriso grub-pc-bin grub-common
```

## 🚀 Building and Running

### 1. Build the Kernel
Compile the source code into a kernel binary (`kfs.bin`):
```bash
make
```

### 2. Create Bootable ISO (uses Docker automatically)
Generate a bootable `kfs.iso` image. If the required tools (`grub-mkrescue`, `xorriso`, `mtools`) are not on the host, this target builds/uses the `kfs-env` Docker image automatically and writes `kfs.iso` into the repo root:
```bash
make iso        # builds/uses the Docker image automatically if needed
```

### 3. Run in QEMU (from ISO only)
The `qemu` target boots the ISO. It depends on `kfs.iso`, so it will trigger `make iso` (and therefore Docker) if the ISO is missing:
```bash
make qemu
```
For headless/terminal-only runs, call QEMU directly with your preferred display flags, e.g.:
```bash
qemu-system-i386 -nographic -serial mon:stdio -cdrom kfs.iso
```
You can also run QEMU inside the Docker image if you prefer to avoid host packages:
```bash
docker build -t kfs-env .
docker run --rm -it -v "$PWD":/workspace kfs-env qemu-system-i386 -nographic -serial mon:stdio -cdrom kfs.iso
```

### Clean
Remove build artifacts:
```bash
make clean
```

## 📂 Project Structure

- `boot.S`: Assembly entry point. Sets up the stack, checks Multiboot magic, and jumps to C code.
- `kernel.c`: Core kernel logic. Handles VGA output, keyboard input, and screen management.
- `linker.ld`: Linker script to define the memory layout of the kernel (load address 1MB).
- `Makefile`: Build automation script.
- `io.h` / `keyboard.h`: Helper headers (inferred).

## 📝 License

This project is open source.
