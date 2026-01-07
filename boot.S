
/* Multiboot Header Constants */
.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide memory map */
.set FLAGS,    ALIGN | MEMINFO  /* this is the Multiboot 'flag' field */
.set MAGIC,    0x1BADB002       /* 'magic number' lets bootloader find the header */
.set CHECKSUM, -(MAGIC + FLAGS) /* checksum of above, to prove we are multiboot */

/* Section multiboot (Header pour le bootloader):
	On a le nombre magique, les flags et le checksum dans cette section.
	Ensuite le script linker place cette partie dans les 8192 premiers octets, ensuite GRUB cherche cette signature quand il l'a trouve
	il sait que le kernel peut etre demarre selon la convention multiboot (saute a _start et passe les pointeurs eax et ebx) */
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/* Section bss (Stack pour le kernel C):
	Dans cette section il n'y a rien elle est mise a 0. On a 2 labels (stack_bottom et stack_top reference a une adresse) separe par 16KiB
	alignee sur 16 (force les adresses suivante a etre des multiples de 16)*/
.section .bss
.align 16
stack_bottom:
.skip 16384 /* 16 KiB */
stack_top:

/* Section text (Point d'entree du kernel):
	On a un fonction _start en assembleur qui sert de point d'entree et qui ensuite appel la fonction kernel_main */
.section .text
/* Rend le mot "_start" visible depuis l'exterieur donc le linker peut y faire reference*/
.global _start
/* Indique que _start est une fonction */
.type _start, @function
/* Fonction _start */
_start:
	/* Initialise le pointeur de pile avec l'adresse de stack_top (partie reserve dans .bss) */
	mov $stack_top, %esp

	/* Met 0 sur le sommet de la stack */
	pushl $0
	/* Prend la valeur qui se trouve sur le sommet de la stack (O via instruction precedente) et initialise EFLAGS avec.
	Ca permet de demarrer avec des FLAGS sans valeurs parasite */
	popf

	/* GRUB place dans EBX l'adresse des multiboots information on place donc cette adresse sur la stack */
	pushl %ebx
	/* GRUB place dans EAX le magic number on place aussi cette info sur la stack notre fonction kernel_main pourrait prendre en argument ebx et eax mais elle ne le fait pas
	on fait ca car on pourrait en avoir besoins pour le futur*/
	pushl %eax

	/* appel de la fonction kernel_main (kernel.c) */
	call kernel_main

	/*Permet de gerer le cas ou kernel_main return (ca ne doit pas arriver normalement) boucle infinie*/
	cli
1:	hlt
	jmp 1b

.size _start, . - _start
