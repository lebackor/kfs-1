/* Defini l'adresse d'entree du binaire comme etant la fonction _start (boot.S). GRUB commencera donc l'execution ici*/
ENTRY(_start)

/* Organise les sections des different .o dans le binaire final*/
SECTIONS
{

	/* On laisse 1MiB pour placer son kernel car la zone avant est utilise pour plein de chose au boot (BIOS, structures, buffers, zones réservées, etc.)*/
	. = 1M;

	/* Place en premier la section multiboot dans la section text du binaire final, c'est necessaire pour que le bootloader (GRUB) detecte le format.
	On place ensuite les sections .text de kernel.o et boot.o (ALIGN(4K) impose que l’adresse de début de cette section .text soit alignée sur 4096 octets) */
	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.multiboot)
		*(.text)
	}

	/* Read-only data. Donnees en lecture seul (Chaine de caractere etc..) regroupe les section rodata des .o dans la section rodata du binaire final*/
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}

	/* .data contient les variables globales ou static avec une valeur initiale non nulle (int i = 42) on dis de mettre toutes les sections data des .o dans
	la section data du binaire final */
	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	}

	/*.bss contient toutes les donnes non initialise (int i; char c). on dis de mettre toutes les sections data des .o dans
	la section data du binaire final.
	En C certaine variable non initialise sont emises comme COMMON on dis ici de les regrouper dans la section bss du binaire final */
	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}
}
