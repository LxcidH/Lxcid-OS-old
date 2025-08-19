ENTRY(main)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    /* The program will be loaded at 0x150000. */
    . = 0x150000;

    .text : {
        *(.text)   /* All code goes here */
    }

    .data : {
        *(.data)   /* All initialized data goes here */
    }

    .bss : {
        *(.bss)    /* All uninitialized data goes here */
    }
}
