#include "elf.h"
#include "../drivers/terminal.h" // For printing errors
#include "../memory/heap.h"      // For malloc/free
#include "../lib/string.h"       // For memcpy and memset

uint32_t elf_load(FAT32_DirectoryEntry* file) {
    if (file == NULL) {
        return 0; // Invalid file entry
    }

    // 1. Read the entire file into a temporary buffer in memory.
    // This is necessary because we need to seek to different parts of the file
    // (program headers, code sections) and the FAT32 driver doesn't support seeking.
    uint8_t* file_buffer = malloc(file->file_size);
    if (!file_buffer) {
        terminal_printf("ELF Error: Not enough memory to load file.\n", FG_RED);
        return 0;
    }
    fat32_read_file(file, file_buffer);

    // 2. The ELF header is at the very beginning of the file buffer.
    // We cast the buffer pointer to the header struct type.
    Elf32_Ehdr* header = (Elf32_Ehdr*)file_buffer;

    // 3. Validate the ELF Magic Number to ensure it's an ELF file.
    if (header->e_ident[0] != ELFMAG0 || header->e_ident[1] != ELFMAG1 ||
        header->e_ident[2] != ELFMAG2 || header->e_ident[3] != ELFMAG3) {
        terminal_printf("ELF Error: Not a valid ELF file.\n", FG_RED);
        free(file_buffer);
        return 0;
    }

    // You could add more validation here, e.g., check for 32-bit, executable type, etc.

    // 4. Get the location of the program header table from the main header.
    Elf32_Phdr* p_headers = (Elf32_Phdr*)(file_buffer + header->e_phoff);

    // 5. Loop through all the program headers.
    for (int i = 0; i < header->e_phnum; i++) {
        Elf32_Phdr* phdr = &p_headers[i];

        // We only care about program headers of type 'PT_LOAD', as these
        // describe segments that need to be loaded into memory.
        if (phdr->p_type == PT_LOAD) {
            // Copy the segment from the file buffer into its target memory location.
            // p_vaddr: The virtual address where the segment should be loaded.
            // file_buffer + p_offset: The location of the segment data within the file.
            // p_filesz: The size of the segment in the file.
            memcpy((void*)phdr->p_vaddr, file_buffer + phdr->p_offset, phdr->p_filesz);

            // The .bss section is uninitialized data. The ELF format specifies this
            // by having p_memsz (memory size) be larger than p_filesz (file size).
            // We must zero out this extra space.
            if (phdr->p_memsz > phdr->p_filesz) {
                uint32_t bss_start = phdr->p_vaddr + phdr->p_filesz;
                uint32_t bss_size = phdr->p_memsz - phdr->p_filesz;
                memset((void*)bss_start, 0, bss_size);
            }
        }
    }

    // The entry point address is stored in the main header.
    uint32_t entry_point = header->e_entry;

    // 6. Clean up the temporary file buffer.
    free(file_buffer);

    // 7. Return the entry point address. The kernel can now jump to this.
    return entry_point;
}

