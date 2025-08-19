#include <stddef.h>
#include "syscall_numbers.h"

// The 'write' function that user programs will call.
int write(int fd, const void* buffer, size_t count) {
    int result;
    asm volatile (
        "int $0x80"         // Add the '$' prefix here
        : "=a" (result)
        : "a" (SYS_WRITE), "b" (fd), "c" (buffer), "d" (count)
    );
    return result;
}

int open(const char* filename) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_OPEN), "b"(filename));
    return result;
}

int read(int fd, void* buffer, size_t count) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(count));
    return result;
}

void clear_screen(void) {
    asm volatile("int $0x80" :: "a"(SYS_CLEAR_SCREEN));
}

void set_cursor(int x, int y) {
    asm volatile("int $0x80" :: "a"(SYS_SET_CURSOR), "b"(x), "c"(y));
}
