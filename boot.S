/*
 * boot.S - Assembly Entry Point
 */

/* Multiboot Header Constants */
.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide memory map */
.set FLAGS,    ALIGN | MEMINFO  /* this is the Multiboot 'flag' field */
.set MAGIC,    0x1BADB002       /* 'magic number' lets bootloader find the header */
.set CHECKSUM, -(MAGIC + FLAGS) /* checksum of above, to prove we are multiboot */

/* Declare the Multiboot header */
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/* Reserve a stack for the C kernel */
.section .bss
.align 16
stack_bottom:
.skip 16384 /* 16 KiB */
stack_top:

/* Kernel Entry Point */
.section .text
.global _start
.type _start, @function
_start:
	/* Set up the stack pointer */
	mov $stack_top, %esp

	/* 
	 * Reset EFLAGS
	 * Pushing 0 and popping it sets EFLAGS to a known state
	 */
	pushl $0
	popf

	/* Push the pointer to the Multiboot information structure (ebx) */
	pushl %ebx
	/* Push the magic value (eax) */
	pushl %eax

	/* Call the kernel main function */
	call kernel_main

	/* 
	 * If the system has nothing more to do, put the computer into an
	 * infinite loop. To be perfectly clear:
	 * 1. Disable interrupts with cli (clear interrupt enable in eflags).
	 *    They are already disabled by the bootloader, so this is not needed
	 *    but it doesn't harm.
	 * 2. Wait for the next interrupt with hlt (halt instruction).
	 *    Since they are disabled, this will lock up the computer.
	 * 3. Jump to the hlt instruction if it ever wakes up.
	 */
	cli
1:	hlt
	jmp 1b

.size _start, . - _start
