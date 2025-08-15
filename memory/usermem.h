#ifndef USERMEM_H
#define USERMEM_H

#include <stdint.h>
#include <stddef.h>

#define USER_SPACE_START 0x100000 // Example: User programs start at 1MB
#define USER_SPACE_END   0x2000000 // Example: User programs have a total of 32MB of space

// Defines the boundaries of user space memory.
// You should change these values to match your OS's memory map.
#define USER_SPACE_START 0x100000
#define USER_SPACE_END   0x2000000

// Function to safely copy data from a user-space buffer to a kernel-space buffer.
// It returns 0 on success and -1 if the provided user address is invalid.
int copy_from_user(void* dest, const void* src, size_t count);

#endif
