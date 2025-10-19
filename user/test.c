#include "lib/syscalls.h" // Assumes you have a header for your syscall wrappers

void welcome(void) {
  const char* msg = "welcome :3\n";
  write(1, msg, 11);
}

void main(void) {
  clear_screen();

  const char* message = "Hello from a C user program!\n";
    const char* msg2 = "test test\n";
    // Call the write syscall:
    // fd = 1 (stdout)
    // buffer = message
    // count = 29 bytes
    write(1, message, 29);
    write(1, msg2, 10);

    // In the future, you will add a sys_exit() call here.
    // For now, the program will just finish and do nothing.
}
