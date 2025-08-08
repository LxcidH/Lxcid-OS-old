ENTRY(start)
SECTIONS
{
    . = 1M; /* Load at 1MB */

    .text :
    {
        *(.multiboot) /* Place the multiboot section first */
        *(.text)
    }

    .data :
    {
        *(.data)
    }

    .bss :
    {
        *(.bss)
    }
}
