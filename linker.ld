/* Set the entry point to our 'start' label from boot.asm */
ENTRY(start)

SECTIONS
{
    /* Begin loading the kernel at the 1MB physical address */
    . = 1M;

    /* Code section (.text) */
    .text :
    {
        *(.multiboot) /* Place the multiboot header FIRST */
        *(.text)
    }

    /* Read-only data section */
    .rodata : { *(.rodata) }

    /* Read-write data section */
    .data : { *(.data) }

    /* Uninitialized data section (BSS) */
    .bss : { *(.bss) }

    /* Symbol for the end of the kernel */
    . = ALIGN(4096);
    kernel_end = .;
}
