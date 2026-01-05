# Makefile for KFS-1

# Compiler and Linker
CC = gcc
AS = as
LD = ld

# Flags
# -m32: Compile for 32-bit x86
# -ffreestanding: Directed to environment without standard library
# -fno-builtin: Don't use built-in functions
# -nostdlib: Don't link standard libraries
CFLAGS = -m32 -ffreestanding -fno-builtin -fno-exceptions -fno-stack-protector -nostdlib -nodefaultlibs -Wall -Wextra
ASFLAGS = --32
LDFLAGS = -m elf_i386 -T linker.ld

# Source files
SOURCES_C = kernel.c
SOURCES_S = boot.S
OBJECTS = $(SOURCES_S:.S=.o) $(SOURCES_C:.c=.o)

# Output binary
KERNEL = kfs.bin
ISO = kfs.iso
DOCKER_IMAGE = kfs-env

# Targets
all: $(KERNEL)

# Link the kernel
$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

# Compile Assembly
%.o: %.S
	$(AS) $(ASFLAGS) -o $@ $<

# Compile C
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Create ISO (Requires grub-mkrescue / xorriso, and a 'grub.cfg')
iso:
	@if [ "$$IN_DOCKER" = "1" ]; then \
		$(MAKE) iso_inner; \
	else \
		docker build -t $(DOCKER_IMAGE) .; \
		docker run --rm -v "$(PWD)":/workspace -w /workspace -e IN_DOCKER=1 $(DOCKER_IMAGE) $(MAKE) iso_inner; \
	fi

iso_inner: $(KERNEL)
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/
	echo 'menuentry "kfs-1" {' > isodir/boot/grub/grub.cfg
	echo '	multiboot /boot/kfs.bin' >> isodir/boot/grub/grub.cfg
	echo '}' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir
	rm -rf isodir

# Run in QEMU (Requires qemu-system-i386)
# Try to run ISO if available, else Kernel directly
qemu: $(KERNEL)
	@if [ -f $(ISO) ]; then \
		qemu-system-i386 -cdrom $(ISO); \
	else \
		echo "Error: ISO not found"; \
	fi

clean:
	rm -f $(OBJECTS) $(KERNEL) $(ISO)
	rm -rf isodir

.PHONY: all clean iso iso_inner qemu
