#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_HEADER_MAGIC         0x1BADB002
// The magic number passed by a muiltiboot compliant bootloader in the EAX register
#define MULTIBOOT_BOOTLOADER_MAGIC      0x2BADB002

// Flags for the 'flags' field of the mulitboot header

// Align all boot modules on i386 page (4KB) boundaries
#define MULTIBOOT_PAGE_ALIGN            (1 << 0)
// Must pass memory info to the kernel
#define MULTIBOOT_MEMORY_INFO           (1 << 1)
// Must pass video info to the kernel
#define MULTIBOOT_VIDEO_MODE            (1 << 2)

// Flags for the 'flags' field of the multiboot info structure

#define MULTIBOOT_INFO_MEMORY           (1 << 0)   // mem_lower and mem_upper are valid
#define MULTIBOOT_INFO_BOOTDEV          (1 << 1)   // boot_device is valid
#define MULTIBOOT_INFO_CMDLINE          (1 << 2)   // cmdline is valid
#define MULTIBOOT_INFO_MODS             (1 << 3)   // mods_count and mods_addr are valid
#define MULTIBOOT_INFO_AOUT_SYMS        (1 << 4)   // a.out symbol table is valid
#define MULTIBOOT_INFO_ELF_SHDR         (1 << 5)   // ELF section header table is valid
#define MULTIBOOT_INFO_MEM_MAP          (1 << 6)   // mmap_addr and mmap_length are valid
#define MULTIBOOT_INFO_DRIVE_INFO       (1 << 7)   // drive_* fields are valid
#define MULTIBOOT_INFO_CONFIG_TABLE     (1 << 8)   // config_table is valid
#define MULTIBOOT_INFO_BOOT_LOADER_NAME (1 << 9)   // boot_loader_name is valid
#define MULTIBOOT_INFO_APM_TABLE        (1 << 10)  // APM table is valid
#define MULTIBOOT_INFO_VBE_INFO         (1 << 11)  // VBE info is valid

// Main multiboot info structure
// Passed by the bootloader to the kernel's entry point
typedef struct multiboot_info
{
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    struct {
        uint32_t num;
        uint32_t size;
        uint32_t addr;
        uint32_t shndx;
    } elf_sec;
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
} __attribute__((packed)) multiboot_info_t;

// Associated sub-structures

// Structure for a single entry in the mem map
typedef struct multiboot_memory_map
{
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_memory_map_t;

// The 'type' for available RAM.
#define MULTIBOOT_MEMORY_AVAILABLE 1

// Struct for a single module loaded by bootloader
typedef struct multiboot_module
{
    uint32_t mod_start;
    uint32_t mod_end;
    const char* string;
    uint32_t reserved;
} __attribute__((packed)) multiboot_module_t;

#endif
