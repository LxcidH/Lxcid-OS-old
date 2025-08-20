#include "multiboot.h"
#include "../drivers/terminal.h"
#include "../idt/idt.h"
#include "../drivers/pic.h"
#include "../drivers/keyboard.h"
#include "../drivers/ide.h"
#include "../fs/fat32.h"
#include "../shell/shell.h"
#include "../memory/pmm.h"
#include "../memory/heap.h"
#include <stdint.h>

// The kernel's main entry point
void kmain(multiboot_info_t* mbi) {
    terminal_initialize();

    // This check is now commented out to bypass the QEMU bug.
    // if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
    //     terminal_writeerror("Invalid Multiboot magic number. Halting.\n");
    //     return;
    // }

    if (!(mbi->flags & MULTIBOOT_INFO_MEM_MAP)) {
        terminal_writeerror("No memory map provided by bootloader. Halting.\n");
        return;
    }

    // --- CORRECT INITIALIZATION ORDER ---
    idt_init();
    pic_remap();
    pmm_init(mbi); // Pass the multiboot info to the PMM
    heap_init();
    fat32_init();
    keyboard_init();

    terminal_welcome();

    asm volatile ("sti");

    shell_init();

    while(1) {
        asm volatile("hlt");
    }
}
// Fix the issue with cp command not being able to copy non-empty files
// IMPLEMENT TEXT EDITOR


