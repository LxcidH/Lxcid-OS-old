#include "syscall.h"
#include "../shell/shell.h" // Or header for your shell/longjmp checkpoint

// Final handler for write (syscall 4) and exit (syscall 1)
void syscall_handler(registers_t* regs) {
    switch (regs->eax) {
        case 1: { // Syscall 1: exit
            // Jump back to the shell's main loop to terminate the program
            longjmp(g_shell_checkpoint, 1);
            break;
        }

        case 4: { // Syscall 4: write
            int fd = regs->ebx;
            char* user_buffer = (char*)regs->ecx;
            size_t count = regs->edx;

            if (fd == 1) { // 1 is stdout
                // A safe OS would copy this data from user space to kernel space
                // before printing to avoid security issues.
                for (size_t i = 0; i < count; i++) {
                    terminal_putchar(user_buffer[i], FG_WHITE);
                }
            }
            break;
        }

        default:
            terminal_printf("Unknown syscall: %d\n", FG_RED, regs->eax);
            longjmp(g_shell_checkpoint, 1); // Terminate on unknown syscall
            break;
    }
}
