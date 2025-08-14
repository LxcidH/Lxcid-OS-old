#include "heap.h"
#include "pmm.h"

static block_header_t *g_heap_start = NULL;

void heap_init() {
    // Request one page from the PMM to start the heap
    g_heap_start = (block_header_t*)pmm_alloc_page();
    if (g_heap_start == NULL) {
        terminal_writeerror("PMM IS OUT OF MEMORY!");
        return;
    }

    // Set up the first block to cover the entire initial page
    g_heap_start->size = PAGE_SIZE - sizeof(block_header_t);        // PAGE_SIZE
    g_heap_start->is_free = true;
    g_heap_start->next = NULL;
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
                new_block->next = current->next;

                current->size = size;
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

    // Get the block header from the data pointer
    block_header_t* header = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    header->is_free = true;

    // TODO: Coalesce with next block if it's also free}
}
