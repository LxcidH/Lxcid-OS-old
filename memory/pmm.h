#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include "../src/multiboot.h" // Include this for the multiboot_info_t struct

#define PAGE_SIZE 4096
// --- Public Function Prototypes ---

// Correct signature for Multiboot
void pmm_init(multiboot_info_t* mbi);

void* pmm_alloc_page(void);
void pmm_free_page(void* ptr);
uint32_t pmm_get_total_pages(void);
uint32_t pmm_get_used_pages(void);
uint8_t pmm_test_page(uint32_t page_num); // Expose this for the memmap command

#endif // PMM_H

