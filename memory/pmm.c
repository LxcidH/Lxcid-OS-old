#include "pmm.h"
#include <stdint.h>
#include "../drivers/terminal.h"

static uint32_t* pmm_bitmap = 0;
static uint32_t pmm_total_pages = 0;
static uint32_t pmm_last_used_page = 0;

extern uint32_t kernel_end;

// --- Internal Helper Functions ---

static void pmm_set_page(uint32_t page_num) {
    if (page_num >= pmm_total_pages) return;
    pmm_bitmap[page_num / 32] |= (1 << (page_num % 32));
}

static void pmm_clear_page(uint32_t page_num) {
    if (page_num >= pmm_total_pages) return;
    pmm_bitmap[page_num / 32] &= ~(1 << (page_num % 32));
}

// --- Public API Functions ---

uint8_t pmm_test_page(uint32_t page_num) {
    if (page_num >= pmm_total_pages) return 1; // Treat out of bounds as used
    return (pmm_bitmap[page_num / 32] >> (page_num % 32)) & 1;
}

// Initializes the PMM using the map from the Multiboot loader
void pmm_init(multiboot_info_t* mbi) {
    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)mbi->mmap_addr;

    // 1. Find the highest available memory address to determine total RAM
    uint64_t highest_addr = 0;
    while ((uint32_t)mmap < mbi->mmap_addr + mbi->mmap_length) {
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint64_t region_end = mmap->addr + mmap->len;
            if (region_end > highest_addr) {
                highest_addr = region_end;
            }
        }
        mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }
    pmm_total_pages = highest_addr / 4096;

    // 2. Place the bitmap right after the kernel
    pmm_bitmap = (uint32_t*)&kernel_end;
    uint32_t bitmap_size = pmm_total_pages / 8;

    // 3. Mark ALL memory as used initially
    // We must clear enough space for the whole bitmap
    uint32_t bitmap_dword_size = (bitmap_size + 3) / 4;
    for (uint32_t i = 0; i < bitmap_dword_size; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }

    // 4. Mark available regions as FREE by re-reading the map
    mmap = (multiboot_memory_map_t*)mbi->mmap_addr;
    while ((uint32_t)mmap < mbi->mmap_addr + mbi->mmap_length) {
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            for (uint64_t j = 0; j < mmap->len; j += 4096) {
                pmm_clear_page((mmap->addr + j) / 4096);
            }
        }
        mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }

    // 5. Mark the kernel's and bitmap's own memory as USED
    uint32_t reserved_pages = (((uint32_t)&kernel_end) + bitmap_size) / 4096 + 1;
    for (uint32_t i = 0; i < reserved_pages; i++) {
        pmm_set_page(i);
    }
}

void* pmm_alloc_page() {
    for (uint32_t i = pmm_last_used_page; i < pmm_total_pages / 32; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            for (int j = 0; j < 32; j++) {
                if (!(pmm_bitmap[i] & (1 << j))) {
                    uint32_t page_num = i * 32 + j;
                    pmm_set_page(page_num);
                    pmm_last_used_page = i;
                    return (void*)(page_num * 4096);
                }
            }
        }
    }
    return 0; // Out of memory
}

void pmm_free_page(void* ptr) {
    uint32_t page_num = (uint32_t)ptr / 4096;
    pmm_clear_page(page_num);
}

uint32_t pmm_get_total_pages(void) {
    return pmm_total_pages;
}

uint32_t pmm_get_used_pages(void) {
    uint32_t used_pages = 0;
    for (uint32_t i = 0; i < pmm_total_pages; i++) {
        if (pmm_test_page(i)) {
            used_pages++;
        }
    }
    return used_pages;
}
