# KFS-1 Architecture Guide

A comprehensive guide to understanding the KFS-1 kernel project. This document explains every concept, file, and function for developers who want to understand or contribute to the codebase.

---

## Table of Contents

1.  [Project Overview](#project-overview)
2.  [Key Concepts](#key-concepts)
3.  [File Structure](#file-structure)
4.  [Boot Flow](#boot-flow)
5.  [Code Walkthrough](#code-walkthrough)
6.  [Building & Running](#building--running)

---

## Project Overview

**KFS-1** (Kernel From Scratch - 1) is a minimal, 32-bit x86 kernel written in C and Assembly. Its purpose is to demonstrate the fundamentals of operating system development:

-   **Bare-metal execution**: The code runs directly on the CPU without any underlying OS.
-   **VGA Text Mode**: Characters are displayed by writing directly to video memory.
-   **Keyboard Input**: Key presses are read by polling hardware I/O ports.
-   **No Standard Library**: `libc` does not exist; all utilities (`memmove`, `strlen`, `printk`) are custom-built.

### Features Implemented (Bonuses)

| Feature | Description |
|---|---|
| **Scrollback History** | A 100-line buffer preserves text that scrolls off the visible 25-line screen. |
| **Virtual Screens (F1-F3)** | Three independent terminal sessions, switchable via function keys. |
| **Color Support** | Each screen has a unique color theme (Grey, Green, Cyan). |
| **Cursor Movement** | Arrow keys navigate within the editable area. |
| **`printk`** | A `printf`-like function supporting `%s`, `%d`, `%x`, `%c`. |
| **Heartbeat Spinner** | A visual indicator (rotating `|/-\`) proving the kernel is running. |

---

## Key Concepts

### 1. Multiboot Specification

The [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html) is a standard that allows any compliant bootloader (like GRUB) to load any compliant kernel. Our kernel uses Multiboot v1.

**How it works:**
1.  GRUB reads the ISO.
2.  It finds a "magic number" (`0x1BADB002`) in the kernel binary's first 8KB.
3.  If found, GRUB loads the kernel into memory and jumps to the entry point (`_start`).

### 2. VGA Text Mode

In text mode, the video card maps an 80x25 character grid to a fixed memory address: `0xB8000`.

**Memory Layout:**
-   Each cell is 2 bytes (1 word):
    -   **Byte 0 (Low):** ASCII character code.
    -   **Byte 1 (High):** Color attribute (4-bit foreground + 4-bit background).
-   Total: `80 cols * 25 rows * 2 bytes = 4000 bytes`.

```c
// Example: Write a white 'A' on blue background at (0, 0)
uint16_t* vga = (uint16_t*)0xB8000;
vga[0] = 'A' | (0x1F << 8); // 0x1F = white (F) on blue (1)
```

### 3. Hardware I/O Ports

The CPU communicates with hardware (keyboard, VGA cursor) via "I/O Ports". These are special addresses accessed with `in` and `out` assembly instructions.

| Port | Purpose |
|---|---|
| `0x60` | Keyboard: Read scancode. |
| `0x64` | Keyboard: Read status register. |
| `0x3D4, 0x3D5` | VGA: Cursor position control. |

### 4. Polling vs. Interrupts

This kernel uses **polling**: it continuously checks the keyboard status port (`0x64`) in a loop to see if a key was pressed. This is simpler but less efficient than **interrupt-driven** I/O (which requires setting up an Interrupt Descriptor Table, or IDT).

---

## File Structure

```
kfs-1/
├── boot.S           # Assembly entry point (Multiboot header, stack setup)
├── kernel.c         # Main kernel logic (terminal, keyboard, printk)
├── linker.ld        # Linker script (memory layout)
├── io.h             # I/O port helpers (inb, outb)
├── keyboard.h       # US keyboard scancode map
├── Makefile         # Build automation
├── Dockerfile       # Docker environment for `grub-mkrescue`
└── ARCHITECTURE.md  # This file
```

---

## Boot Flow

The execution sequence from power-on to the main loop is:

```
┌─────────────────┐
│      BIOS       │
└────────┬────────┘
         │ Loads GRUB from ISO
         ▼
┌─────────────────┐
│      GRUB       │
└────────┬────────┘
         │ Finds Multiboot header
         │ Loads kfs.bin to memory (at 1MB)
         │ Jumps to `_start`
         ▼
┌─────────────────┐
│ boot.S: _start  │
└────────┬────────┘
         │ 1. Set stack pointer (`esp = stack_top`)
         │ 2. Reset EFLAGS
         │ 3. Push Multiboot info (eax, ebx)
         │ 4. `call kernel_main`
         ▼
┌─────────────────┐
│   kernel_main   │
└────────┬────────┘
         │ 1. `terminal_initialize()`
         │ 2. Print welcome message
         │ 3. `set_input_boundary()`
         │ 4. `cli` (disable interrupts)
         │ 5. Enter infinite loop:
         │    ├── `keyboard_handler()`
         │    └── Update heartbeat spinner
         ▼
┌─────────────────┐
│  Infinite Loop  │
└─────────────────┘
```

---

## Code Walkthrough

### File: `boot.S`

**Purpose:** The Multiboot header and assembly entry point.

| Section/Symbol | Description |
|---|---|
| `.multiboot` | Contains the magic number (`0x1BADB002`), flags, and checksum for GRUB. |
| `.bss: stack_bottom / stack_top` | Reserves 16KB for the kernel stack. |
| `.text: _start` | The entry point. Sets up the stack, pushes Multiboot info to the C function, and calls `kernel_main`. |
| `cli; hlt` | After `kernel_main` returns (it never should), halt the CPU. |

---

### File: `linker.ld`

**Purpose:** Tells the linker how to arrange sections in the final binary.

-   **`ENTRY(_start)`:** Specifies the entry point symbol.
-   **`. = 1M`:** Places the kernel at the 1MB physical address mark (standard for x86 kernels).
-   **Section Order:** `.text` (code) → `.rodata` (read-only data) → `.data` (initialized data) → `.bss` (uninitialized data/stack).

---

### File: `io.h`

**Purpose:** Inline assembly wrappers for I/O port access.

| Function | Description |
|---|---|
| `outb(port, val)` | Write byte `val` to I/O `port`. |
| `inb(port)` | Read and return a byte from I/O `port`. |

---

### File: `keyboard.h`

**Purpose:** Maps scancodes (hardware codes from the keyboard) to ASCII characters.

-   `kbdus[128]`: An array where `kbdus[scancode]` gives the ASCII character.
-   Special keys (like Backspace, Tab, Enter) are mapped to their escape codes (`\b`, `\t`, `\n`).
-   Function keys (F1-F12), arrow keys, etc., are handled separately in `keyboard_handler()`.

---

### File: `kernel.c`

**Purpose:** The main kernel logic. This is the heart of the project.

#### Data Structures

| Name | Type | Description |
|---|---|---|
| `ScreenState` | `struct` | Holds all state for a virtual screen: cursor position (`row`, `column`), viewport (`view_row`), color, history buffer, and input protection boundary. |
| `screens[3]` | `ScreenState[]` | Array of 3 virtual screen states. |
| `current_screen` | `int` | Index of the currently active screen. |
| `terminal_row`, `terminal_column`, etc. | Global | "Working copy" of the current screen's state, used by all functions. |
| `vga_buffer` | `uint16_t*` | Pointer to VGA memory at `0xB8000`. |

#### Functions

| Function | Line | Description |
|---|---|---|
| `vga_entry_color(fg, bg)` | 50 | Combines foreground and background colors into a single byte. |
| `vga_entry(char, color)` | 54 | Combines a character and a color byte into a 16-bit VGA word. |
| `memmove(dst, src, size)` | 59 | Custom memory copy (overlapping-safe). Required since no libc. |
| `strlen(str)` | 72 | Custom string length function. |
| `update_cursor(x, y)` | 81 | Uses I/O ports `0x3D4`/`0x3D5` to set the hardware cursor position. Hides cursor if off-screen. |
| `refresh_screen()` | 105 | Copies the current viewport (25 lines from history buffer) to VGA memory. |
| `terminal_initialize()` | 119 | Initializes all 3 screens with blank buffers, default colors, and zero positions. |
| `terminal_scroll()` | 151 | Handles scrolling. Shifts the 100-line history buffer up if full. Otherwise, just adjusts the viewport. |
| `set_input_boundary()` | 184 | Saves the current cursor position as the "no delete past here" point. |
| `terminal_putchar(c)` | 189 | Writes a character to the buffer. Handles `\n` (newline), `\b` (backspace with smart wrap and ripple delete), and normal characters. Calls `terminal_scroll()` and `refresh_screen()`. |
| `terminal_write(data, size)` | 286 | Writes a string of `size` characters. |
| `terminal_writestring(data)` | 291 | Writes a null-terminated string. |
| `printk(format, ...)` | 297 | A `printf`-like function. Supports `%c`, `%s`, `%d`, `%x`. Uses `va_list` for variadic arguments. |
| `switch_screen(index)` | 354 | Saves current screen state to `screens[]`, loads state from `screens[index]`, and calls `refresh_screen()`. |
| `keyboard_handler()` | 381 | Polls keyboard port. Handles F1-F3 (screen switch), arrow keys (cursor move), Page Up/Down (viewport scroll), and normal typing. |
| `kernel_main()` | 474 | Entry point called from `boot.S`. Initializes terminal, prints welcome message, sets input boundary, and enters the main polling loop. |

#### Backspace Logic (Deep Dive)

The backspace (`\b`) handler in `terminal_putchar()` is complex:

1.  **Protection Check:** If the cursor is at or before the input boundary, do nothing.
2.  **Full Line Edge Case:** If at column 79 with a character, delete it in place (don't move left first). This handles lines that wrap perfectly.
3.  **Ripple Delete:** Move all characters on the line left by one to fill the gap.
4.  **Heartbeat Protection:** On row 0, the ripple delete stops at column 78 to avoid corrupting the spinner at column 79.
5.  **Backspace Wrap:** If at column 0, scan the previous line for the last non-space character. Jump to `found_col + 1`. Ignore the spinner position `(0, 79)`.
6.  **Auto-Scroll Up:** If the cursor moves above the current viewport, scroll the viewport up.

---

## Building & Running

### Prerequisites

-   **Compiler:** `gcc` (with 32-bit support: `gcc-multilib`)
-   **Assembler:** `as` (GNU Binutils)
-   **Linker:** `ld` (GNU Binutils)
-   **ISO Creation:** `docker` (to run `grub-mkrescue` in a container)
-   **Emulator:** `qemu-system-i386`

### Commands

| Command | Description |
|---|---|
| `make` | Compiles `boot.S` and `kernel.c`, links into `kfs.bin`. |
| `make iso` | Builds the ISO using Docker for GRUB. Produces `kfs.iso`. |
| `make qemu` | Runs the ISO in QEMU. |
| `make clean` | Removes all build artifacts. |

### Quick Start

```bash
make clean
make iso
make qemu
```

---

## Conclusion

This project demonstrates the foundational concepts of OS development. By reading through the code and this guide, you should now understand:

-   How a bootloader finds and loads a kernel.
-   How to write directly to hardware (VGA, keyboard).
-   How to build a basic I/O system without any standard library.
-   How to manage multiple virtual screens with state isolation.

For further exploration, consider adding:
-   An Interrupt Descriptor Table (IDT) for interrupt-driven keyboard handling.
-   Paging for virtual memory.
-   A simple shell or command interpreter.
