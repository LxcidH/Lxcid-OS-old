#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define HEAP_SIZE_PAGES 1024
#define TOTAL_HEAP_SIZE HEAP_SIZE_PAGES * PAGE_SIZE


typedef struct block_header {
    size_t size;                    // Size of the data block (excluding this header)
    bool is_free;                   // Flag to indicate if the block is free
    struct block_header *next;      // Ptr to the next block in the list
} block_header_t;

void heap_init();
void* malloc(size_t size);
void free(void* ptr);

#endif
