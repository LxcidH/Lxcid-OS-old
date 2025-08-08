#include "shell.h"

#define CMD_BUFFER_SIZE 256
static char cmd_buffer[CMD_BUFFER_SIZE];
static size_t buffer_index = 0;

void shell_init(void) {
    buffer_index = 0;
    terminal_writestring("\n LxcidOS > "); // Inital prompt
}

void shell_handle_key(char c) {
    // Handle backspace
    if (c == '\b') {
        if (buffer_index > 0) {
            buffer_index--;
            terminal_putchar('\b');
        }
        return;
    }

    // Handle Enter key
    if (c == '\n') {
        cmd_buffer[buffer_index] = '\0'; // Null-terminate the command
        terminal_putchar('\n');
        // TODO: process the command in the buffer

        buffer_index = 0;
        terminal_writestring("LxcidOS > ");
        return;
    }

    // Handle all other characters
    if (buffer_index < CMD_BUFFER_SIZE - 1) {
        cmd_buffer[buffer_index++] = c;
        terminal_putchar(c);    // Echo char to screen
    }
}
