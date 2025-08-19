#include "lib/syscalls.h" // Assumes you have a header for your syscall wrappers

void main(void) {
    const char* message = "Hello from a C user program!\n";

    // Call the write syscall:
    // fd = 1 (stdout)
    // buffer = message
    // count = 29 bytes
    write(1, message, 29);

    // In the future, you will add a sys_exit() call here.
    // For now, the program will just finish and do nothing.
}
