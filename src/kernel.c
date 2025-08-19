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

    /*
    fat32_list_root_dir();

    FAT32_DirectoryEntry* file_entry = fat32_find_entry("HELLO.TXT");
    if (file_entry != NULL) {
        terminal_printf("Found HELLO.TXT! Size: %d bytes\n", FG_MAGENTA, file_entry->file_size);

        // Allocate a buffer and read the file
        uint8_t* buffer = (uint8_t*)malloc(file_entry->file_size);
        fat32_read_file(file_entry, buffer);

        // Print the content to verify
        terminal_printf("Content: %s\n", FG_GREEN, (char*)buffer);

        free(buffer);
    } else {
        terminal_printf("HELLO.TXT Not FOUND\n", FG_WHITE);
    }
    */
    while(1) {
        asm volatile("hlt");
    }
}
// RE-ADD SYSCALL
// IMPLEMENT TEXT EDITOR


