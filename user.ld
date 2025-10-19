
ENTRY(_start)

/* Define the output format for our 32-bit ELF executable. */
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    /*
     * We specify where the program should be loaded in memory.
     * 0x150000 is the address you've chosen.
     */
    . = 0x150000;

    /* Place the executable code section first. */
    .text : {
        *(.text)    /* All code goes here */
    }

    /* Place initialized data next. */
    .data : {
        *(.data)    /* All initialized data goes here */
    }

    /* Finally, reserve space for uninitialized data. */
    .bss : {
        *(.bss)     /* All uninitialized data goes here */
    }
}


