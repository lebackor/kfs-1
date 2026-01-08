CC = gcc # Compilateur C
AS = as # Assembleur GNU
LD = ld # Linker GNU

# CFLAGS :
#   -m32                : cible x86 32-bit
#   -ffreestanding      : environnement "freestanding" (pas d'OS / pas de libc)
#   -fno-builtin        : interdit les builtins implicites (memcpy, etc.)
#   -fno-stack-protector: désactive la protection de pile (souvent dépendante de libc/runtime)
#   -nostdlib           : ne pas lier les libs standard
#   -nodefaultlibs      : ne pas lier les libs par défaut
#   -Wall -Wextra       : warnings utiles
CFLAGS = -m32 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -fno-exceptions -fno-rtti -nodefaultlibs -Wall -Wextra

# ASFLAGS :
#   --32 : assemble en 32-bit
ASFLAGS = --32

# LDFLAGS :
#   -m elf_i386 : format ELF 32-bit (i386)
#   -T linker.ld: script de link custom (layout mémoire/sections)
LDFLAGS = -m elf_i386 -T linker.ld

# Sources / Objets
SOURCES_C = kernel.c
SOURCES_S = boot.S
OBJECTS = $(SOURCES_S:.S=.o) $(SOURCES_C:.c=.o)

# Output
KERNEL = kfs.bin
ISO = kfs.iso
DOCKER_IMAGE = kfs-env

# QEMU
#   Lance l’ISO avec KVM (Utilisation du CPU de la machine physique pas emulation via QEMU) (reconstruit si besoin via la dépendance $(ISO)).
qemu: $(ISO)
	qemu-system-i386 -enable-kvm -cdrom $(ISO)

# Targets
all: $(KERNEL)

# Link (final)
#   Le binaire final (kfs.bin) est produit en liant les objets ASM + C (boot.o et kernel.o).
#   Dépendances implicites : si boot.o ou kernel.o change, kfs.bin est relinké.
$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

# Build des objets
%.o: %.S
	$(AS) $(ASFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ISO bootable avec docker (GRUB)
#   grub-mkrescue dépend de paquets (xorriso, grub-tools, etc.)
#   qui ne sont pas installables facilement sur les machines.
#
# Fonctionnement :
#   - build l'image Docker
#   - lance un container en montant le repo courant dans /workspace sur le docker
#   - exécute la "iso_inner" dans un environnement qui possede les paquets necessaire
iso: $(ISO)

$(ISO): $(KERNEL)
	docker build -t $(DOCKER_IMAGE) .
	docker run --rm -v ./:/workspace -w /workspace $(DOCKER_IMAGE) $(MAKE) iso_inner

# Construction interne de l'ISO :
#   1) Prépare l'arborescence GRUB : isodir/boot/grub
#   2) Copie le kernel dans isodir/boot/
#   3) Génère un grub.cfg minimal
#   4) Construit l'ISO via grub-mkrescue
#   5) Nettoie le dossier temporaire
iso_inner:
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/
	echo 'menuentry "kfs-1" {' > isodir/boot/grub/grub.cfg
	echo '	multiboot /boot/kfs.bin' >> isodir/boot/grub/grub.cfg
	echo '}' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir
	rm -rf isodir

clean:
	rm -f $(OBJECTS) $(KERNEL) $(ISO)
	rm -rf isodir

.PHONY: all clean iso iso_inner qemu


# kfs.bin = boot.o + kernel.o (assemble par le linker)
# kfs.iso = kfs.bin + grub.cfg (assemble par grub-mkrescue qui ajoute l’amorce GRUB pour rendre l’ISO bootable.)
