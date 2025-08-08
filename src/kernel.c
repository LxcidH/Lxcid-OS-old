#include "multiboot.h"
#include "../drivers/terminal.h"
#include "../idt/idt.h"
#include <stdint.h>

// The kernel's main entry point
void kmain(multiboot_info_t* mbi) {
    // Init our terminal driver
    terminal_initialize();
    idt_init();
    idt_load();

    terminal_welcome();

    // Print some text to test it
   terminal_writestring("LxcidOS is running!\n");
    terminal_writestring("The Interrupt Descriptor Table (IDT) has been loaded.");
}


