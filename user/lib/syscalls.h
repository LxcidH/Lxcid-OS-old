#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stddef.h> // For size_t

// --- Function Prototypes for User-Space Programs ---

int write(int fd, const void* buffer, size_t count);
int open(const char* filename);
int read(int fd, void* buffer, size_t count);
void clear_screen(void);
void set_cursor(int x, int y);
void exit(void); 
int get_key(void);

#endif // SYSCALLS_H
