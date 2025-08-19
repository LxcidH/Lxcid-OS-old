#include "syscall.h"
#include "../shell/shell.h" // Or header for your shell/longjmp checkpoint
#include "../drivers/terminal.h"
#include "../drivers/keyboard.h"
#include "../user/lib/syscall_numbers.h"
#include "../fs/fat32.h"

static int kernel_sys_write(registers_t* regs);
static int kernel_sys_open(registers_t* regs);
static int kernel_sys_read(registers_t* regs);
static void kernel_sys_clear_screen(void);
static void kernel_sys_set_cursor(registers_t* regs);
// Final handler for write (syscall 4) and exit (syscall 1)
void syscall_handler(registers_t* regs) {
    switch (regs->eax) {
        case SYS_EXIT: { // Syscall 1: exit
            // Jump back to the shell's main loop to terminate the program
            longjmp(g_shell_checkpoint, 1);
            break;
        }

        case SYS_WRITE: { // NEW CASE
            int bytes_written = kernel_sys_write(regs);
            regs->eax = bytes_written; // Put the return value back in EAX
            break;
        }

        case SYS_OPEN: {
            regs->eax = kernel_sys_open(regs); // Put return value in EAX
            break;
        }
        case SYS_READ: {
            regs->eax = kernel_sys_read(regs); // Put return value in EAX
            break;
        }
        case SYS_CLEAR_SCREEN:
            kernel_sys_clear_screen();
            break;
        case SYS_SET_CURSOR:
            kernel_sys_set_cursor(regs);
            break;

        default:
            terminal_printf("Unknown syscall: %d\n", FG_RED, regs->eax);
            longjmp(g_shell_checkpoint, 1); // Terminate on unknown syscall
            break;
    }
}

// This is the kernel's internal function for writing.
// It returns the number of bytes written.
static int kernel_sys_write(registers_t* regs) {
    int fd = regs->ebx;             // File descriptor
    const char* buffer = (const char*)regs->ecx; // Pointer to user's data
    size_t count = regs->edx;       // How many bytes to write

    // For now, we only handle fd 1, which is standard output (the screen).
    // A real OS would check permissions and that the user buffer is valid.
    if (fd == 1) {
        for (size_t i = 0; i < count; i++) {
            terminal_putchar(buffer[i], FG_WHITE);
        }
        return count; // Return the number of bytes we successfully wrote.
    }

    // Later, you would add logic for writing to actual files here.
    return -1; // Return -1 for an error (e.g., bad file descriptor)
}

// Kernel-side implementation for 'open'
static int kernel_sys_open(registers_t* regs) {
    const char* filename = (const char*)regs->ebx;

    // For now, we'll just find the file. A real OS would create a file
    // descriptor and track open files in a table.
    FAT32_DirectoryEntry* file = fat32_find_entry(filename, g_current_directory_cluster);

    if (file == NULL) {
        return -1; // Return -1 for "file not found"
    }

    // A real OS would return a file descriptor (e.g., 3, 4, 5...).
    // For our simple editor, we can cheat and return the file's starting cluster.
    // This is NOT a secure or robust design, but it's simple and works for now.
    uint32_t cluster = (file->fst_clus_hi << 16) | file->fst_clus_lo;
    return cluster;
}

// Kernel-side implementation for 'read'
static int kernel_sys_read(registers_t* regs) {
    int fd = regs->ebx;
    void* buffer = (void*)regs->ecx;
    size_t count = regs->edx;

    // We no longer handle stdin (fd=0) for now.
    // This is just for reading from files.
    FAT32_DirectoryEntry* file = fat32_find_entry_by_cluster(fd);
    if (file == NULL) {
        return -1;
    }

    if (count > file->file_size) {
        count = file->file_size;
    }

    fat32_read_file(file, buffer);
    return count;
}

static void kernel_sys_clear_screen(void) {
    terminal_initialize(); // Your existing function to clear the screen
}

static void kernel_sys_set_cursor(registers_t* regs) {
    int x = regs->ebx;
    int y = regs->ecx;
    terminal_set_cursor(x, y); // Your existing function to move the cursor
}
