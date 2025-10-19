#include "syscalls.h"
#include <stddef.h>
#include "syscall_numbers.h" // Assuming this file contains your SYS_* defines

/**
 * @brief Issues a 'write' system call.
 * @param fd File descriptor (e.g., 1 for stdout).
 * @param buffer Pointer to the data to write.
 * @param count Number of bytes to write.
 * @return The number of bytes written, or an error code.
 */
int write(int fd, const void* buffer, size_t count) {
    int result;
    // We use specific register constraints ('a', 'b', 'c', 'd') to ensure
    // the C variables are loaded into the correct registers (eax, ebx, ecx, edx)
    // before the 'int $0x80' instruction is executed.
    asm volatile (
        "int $0x80"
        : "=a" (result) // Output: the return value from the syscall goes into 'result'
        : "a" (SYS_WRITE), "b" (fd), "c" (buffer), "d" (count) // Inputs
        : "memory" // Clobber list: tells the compiler memory might be changed
    );
    return result;
}

/**
 * @brief Issues an 'open' system call.
 * @param filename Null-terminated string of the file to open.
 * @return A file descriptor on success, or an error code.
 */
int open(const char* filename) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a" (result)
        : "a" (SYS_OPEN), "b" (filename)
        : "memory"
    );
    return result;
}

/**
 * @brief Issues a 'read' system call.
 * @param fd File descriptor to read from.
 * @param buffer Pointer to the buffer to store the read data.
 * @param count Maximum number of bytes to read.
 * @return The number of bytes read, or an error code.
 */
int read(int fd, void* buffer, size_t count) {
    int result;
    asm volatile(
        "int $0x80"
        : "=a" (result)
        : "a" (SYS_READ), "b" (fd), "c" (buffer), "d" (count)
        : "memory"
    );
    return result;
}

/**
 * @brief Issues a 'clear_screen' system call.
 */
void clear_screen(void) {
    // No return value, so the output constraint is empty.
    asm volatile("int $0x80" : : "a"(SYS_CLEAR_SCREEN));
}

/**
 * @brief Issues a 'set_cursor' system call.
 * @param x The column for the cursor.
 * @param y The row for the cursor.
 */
void set_cursor(int x, int y) {
    asm volatile("int $0x80" : : "a"(SYS_SET_CURSOR), "b"(x), "c"(y));
}

/**
 * @brief Issues an 'exit' system call to terminate the current program.
*/
void sys_exit(void) {
    // We assume the syscall number for exit is 1. Make sure this matches
    // your kernel's syscall handler.
    asm volatile("int $0x80" : : "a"(SYS_EXIT));
}

int get_key(void) {
  int key;
  asm volatile("int $0x80" : "=a"(key) : "a"(SYS_GET_KEY));
  return key;
}
