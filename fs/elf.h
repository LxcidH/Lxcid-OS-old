#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include "fs/fat32.h" // Depend on the concrete FAT32 implementation

// --- ELF Data Types ---
typedef uint32_t Elf32_Addr; // Unsigned program address
typedef uint32_t Elf32_Off;  // Unsigned file offset
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Word;
typedef uint16_t Elf32_Half;

// --- ELF Header (Ehdr) ---
// This is the main header at the start of the file. It provides the roadmap
// to the rest of the file's contents.
#define EI_NIDENT 16
typedef struct {
    unsigned char e_ident[EI_NIDENT]; // Magic number and other info
    Elf32_Half    e_type;             // Object file type
    Elf32_Half    e_machine;          // Architecture
    Elf32_Word    e_version;          // Object file version
    Elf32_Addr    e_entry;            // Entry point virtual address
    Elf32_Off     e_phoff;            // Program header table file offset
    Elf32_Off     e_shoff;            // Section header table file offset
    Elf32_Word    e_flags;            // Processor-specific flags
    Elf32_Half    e_ehsize;           // ELF header size in bytes
    Elf32_Half    e_phentsize;        // Program header table entry size
    Elf32_Half    e_phnum;            // Program header table entry count
    Elf32_Half    e_shentsize;        // Section header table entry size
    Elf32_Half    e_shnum;            // Section header table entry count
    Elf32_Half    e_shstrndx;         // Section header string table index
} Elf32_Ehdr;

// --- Program Header (Phdr) ---
// An entry in the program header table, describing a segment or other
// information the system needs to prepare the program for execution.
typedef struct {
    Elf32_Word p_type;   // Segment type
    Elf32_Off  p_offset; // Segment file offset
    Elf32_Addr p_vaddr;  // Segment virtual address
    Elf32_Addr p_paddr;  // Segment physical address (if applicable)
    Elf32_Word p_filesz; // Segment size in file
    Elf32_Word p_memsz;  // Segment size in memory
    Elf32_Word p_flags;  // Segment flags
    Elf32_Word p_align;  // Segment alignment
} Elf32_Phdr;


// --- ELF Identification Constants (e_ident) ---
#define ELFMAG0 0x7F // e_ident[0]
#define ELFMAG1 'E'  // e_ident[1]
#define ELFMAG2 'L'  // e_ident[2]
#define ELFMAG3 'F'  // e_ident[3]

// --- Segment Types (p_type) ---
#define PT_NULL    0 // Unused
#define PT_LOAD    1 // A loadable segment
#define PT_DYNAMIC 2 // Dynamic linking information
#define PT_INTERP  3 // Interpreter information

/**
 * @brief Loads and validates an ELF executable from a FAT32 directory entry.
 *
 * This function reads the ELF headers from the file, validates it, and copies
 * the loadable segments into memory at their specified virtual addresses.
 *
 * @param file A pointer to the FAT32 directory entry of the executable.
 * @return The virtual address of the program's entry point, or 0 on failure.
 */
uint32_t elf_load(FAT32_DirectoryEntry* file);

#endif // ELF_H

