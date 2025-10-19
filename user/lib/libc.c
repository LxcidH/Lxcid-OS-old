#include "syscalls.h"
#include "syscall_numbers.h"

/**
 * @brief Reads a single character from standard input (the keyboard).
 * This is a standard C library function that uses the 'read' syscall.
 * @return The character read.
 */
int getchar() {
    char c;
    // Use the read syscall to get 1 character from stdin (fd=0).
    read(0, &c, 1);
    return (int)c;
}

/**
 * @brief Terminates the program using the exit syscall.
 * This is a standard C library function that uses the 'exit' syscall.
 */
void exit() {
    // This calls the low-level syscall wrapper for SYS_EXIT.
    sys_exit();
}

