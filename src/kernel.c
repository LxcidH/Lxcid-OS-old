#include "multiboot.h"
#include "../drivers/terminal.h"
#include <stdint.h>

// The kernel's main entry point
void kmain(multiboot_info_t* mbi) {
    // Init our terminal driver
    terminal_initialize();

    // Print some text to test it
    terminal_writestring("Hello, 32bit kernel world!\n");
}


