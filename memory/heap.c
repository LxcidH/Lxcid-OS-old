#include "heap.h"
#include "pmm.h"
#include "../drivers/terminal.h"
static block_header_t *g_heap_start = NULL;

void heap_init() {
    // We want a 2.048 MB heap.
    // 2.048 MB = 2048 KB = 2,097,152 bytes.
    // 2,097,152 bytes / 4096 bytes_per_page = 512 pages.

    // Request the first page from the PMM to start the heap
    void* heap_page_ptr = pmm_alloc_page();

    if (heap_page_ptr == NULL) {
        terminal_writeerror("PMM IS OUT OF MEMORY!");
        return;
    }

    g_heap_start = (block_header_t*)heap_page_ptr;

    // We must now allocate the remaining pages and assume they are contiguous.
    for (size_t i = 1; i < HEAP_SIZE_PAGES; i++) {
        if (pmm_alloc_page() == NULL) {
            terminal_writeerror("PMM IS OUT OF MEMORY!");
            // TODO: Free the pages we already allocated.
            return;
        }
    }

    // Set up the first block to cover the entire 2.048 MB heap.
    // The size is the total heap size minus the size of the block header itself.
    g_heap_start->size = TOTAL_HEAP_SIZE - sizeof(block_header_t);
    g_heap_start->is_free = true;
    g_heap_start->next = NULL;

    // terminal_printf("Heap initialized with a size of %d bytes.\n", FG_YELLOW, TOTAL_HEAP_SIZE);
    // terminal_printf("Heap start addr: %x\n", FG_GREEN, g_heap_start);
}

void* malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    block_header_t *current = g_heap_start;

    while(current != NULL) {
        // Find the first free block that is large enough
        if (current->is_free && current->size >= size) {
            // Can we split this block?
            // Check if there's enough space for a new block header and some data
            if (current->size > size + sizeof(block_header_t)) {
                block_header_t *new_block = (block_header_t*)((uint8_t*)current + sizeof(block_header_t) + size);
                new_block->size = current->size - size - sizeof(block_header_t);
                new_block->is_free = true;

                // Fix: The new block's next pointer should point to the block that was originally
                // after the 'current' block.
                new_block->next = current->next;

                current->size = size;
                // Fix: The current block's next pointer should point to the new block.
                current->next = new_block;
            }

            current->is_free = false;
            // Return a ptr to the data region, which is right after the header
            return (void*)((uint8_t*)current + sizeof(block_header_t));
        }
        current = current->next;
    }

    // TODO: No suitable block found. need to expand the heap by calling pmm_alloc_page()
    // and adding the new memoryt to the end of the list. FOr now, we fail/
    return NULL;
}

void free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    block_header_t* header = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    header->is_free = true;

    // Check for a free block immediately preceding this one.
    block_header_t* current = g_heap_start;
    block_header_t* prev_block = NULL;

    while (current != header && current != NULL) {
        prev_block = current;
        current = current->next;
    }

    // Coalesce with the previous block if it's also free.
    if (prev_block != NULL && prev_block->is_free) {
        prev_block->size += header->size + sizeof(block_header_t);
        prev_block->next = header->next;
        header = prev_block; // The 'new' header is now the previous block.
    }

    // Coalesce with the next block if it's also free.
    if (header->next != NULL && header->next->is_free) {
        header->size += header->next->size + sizeof(block_header_t);
        header->next = header->next->next;
    }
}
