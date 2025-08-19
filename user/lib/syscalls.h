#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stddef.h> // For size_t

// Prototypes for your user-space syscall wrappers
int write(int fd, const void* buffer, size_t count);
// Add other syscalls like exit(), open(), etc. here
int open(const char* filename);
int read(int fd, void* buffer, size_t count);
void clear_screen(void);
void set_cursor(int x, int y);
#endif
