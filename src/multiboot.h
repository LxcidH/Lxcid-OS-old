#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_HEADER_MAGIC         0x1BADB002
#define MULTIBOOT_HEADER_FLAGS         ((1<<0) | (1<<1))
#define MULTIBOOT_CHECKSUM             -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    // ... other fields we don't need yet
} multiboot_info_t;

#endif
