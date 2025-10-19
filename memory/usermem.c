#include "../lib/string.h" // For memcpy
#include "usermem.h"
// Define the boundaries of user space memory

int copy_from_user(void* dest, const void* src, size_t count) {
    // Check if the entire requested memory block is within the user space boundaries
    const uint8_t* user_start = (const uint8_t*)src;
    const uint8_t* user_end = user_start + count;

    if (user_start < (const uint8_t*)USER_SPACE_START || user_end > (const uint8_t*)USER_SPACE_END) {
        // The memory region is outside the allowed user space
        return -1;
    }

    // Since the address is validated, it's safe to perform the copy
    memcpy(dest, src, count);

    return 0; // Success
}
