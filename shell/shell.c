#include "shell.h"
#include "../lib/string.h"
#include "../io/io.h"
#include "../drivers/keyboard.h"
#include "../memory/pmm.h"
// Forward declarations for command handlers (static cus they're internal)
static void cmd_help(int argc, char* argv[]);
static void cmd_echo(int argc, char* argv[]);
static void cmd_reboot(int argc, char* argv[]);
static void cmd_memmap(int argc, char* argv[]);
static void cmd_clear(int argc, char* argv[]);
static void cmd_peek(int argc, char* argv[]);
static void cmd_poke(int argc, char* argv[]);

// The command structure definition (internal)
typedef struct {
    const char* name;
    void (*handler)(int argc, char* argv[]);
    const char* help_text;
} shell_command_t;

// The command table (static, so it's private to this file)
static const shell_command_t commands[] = {
    {"help", cmd_help, "Displays the help message.\n"},
    {"echo", cmd_echo, "Prints back the args.\n"},
    {"reboot", cmd_reboot, "Reboots the computer.\n"},
    {"memmap", cmd_memmap, "Prints the current memory usage and a memory map.\n"},
    {"clear", cmd_clear, "Clears the terminal.\n"},
    {"peek", cmd_peek, "Reads a 32-bit value from a memory address.\n"},
    {"poke", cmd_poke, "Writes a 32-bit value to a memory address.\n"}
};
static const int num_commands = sizeof(commands) / sizeof(shell_command_t);

#define CMD_BUFFER_SIZE 256
#define PROMPT "LxcidOS >"
#define MAX_ARGS 16

static char cmd_buffer[CMD_BUFFER_SIZE];
static size_t buffer_index = 0;

/**
 * @brief Parses a command string into arguments (argc/argv).
 * @details This function modifies the input string by replacing spaces with
 * null terminators to create an array of C-strings.
 * * @param input The mutable command string to parse.
 * @param argc  A pointer to an integer where the argument count will be stored.
 * @param argv  An array to be filled with pointers to the start of each argument.
 * @param max_args The maximum number of arguments the argv array can hold.
 */
static void parse_command(char* input, int* argc, char** argv, int max_args) {
    *argc = 0;

    // As long as we haven't reached the end of the string or the argument limit
    while(*input != '\0' && *argc < max_args - 1) {
        // 1. Skip all leading whitespace
        while(*input == ' ' || *input == '\t' || *input == '\n') {
            *input++ = '\0'; // Also replace it with null just incase
        }

        // If we're not at the end of the string, we have found a new arg
        if(*input != '\0') {
            // 2. Store a ptr to the beginning of the arg
            argv[(*argc)++] = input;
        }

        // 3. Scan forward until the next whitespace or the end of the string
        while(*input != '\0' && *input != ' ' && *input != '\t' && *input != '\n') {
            input++;
        }
    }
    // 4. Null-terminate the arg list itself, which is standard convention
    argv[*argc] = NULL;
}


// Internal Helper funcs
// The command processing logic (internal)
static void process_command(char* buffer) {
    char* argv[MAX_ARGS];
    int argc = 0;

    parse_command(buffer, &argc, argv, MAX_ARGS);

    // If no cmd was entered, just return
    if (argc == 0) {
        return;
    }

    // Find and execute the command
    for(int i = 0; i < num_commands; i++) {
        if(strcmp(argv[0], commands[i].name) == 0) {
            commands[i].handler(argc, argv);
            return;
        }
    }
    terminal_writeerror("Command %s not found!\n", buffer);
}

// Command Handler Implementations (internal)
static void cmd_help(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    terminal_printf("List of available commands:\n", FG_MAGENTA);
    for(int i = 0; i < num_commands; i++) {
        // CHANGE: Pass the 'name' member of the struct, not the whole struct.
        terminal_printf("  %s - %s\n", FG_WHITE, commands[i].name, commands[i].help_text);
    }
}

static void cmd_echo(int argc, char* argv[]) {
    // Start at index 1 to skip the command name itself
    for(int i = 1; i < argc; i++) {
        // Print the current arg string
        terminal_printf("%s", FG_WHITE, argv[i]);

        // If this is not the last arg, print a space after it
        if (i < argc - 1) {
            terminal_putchar(' ', FG_WHITE);
        }
    }
    // Print newline after all args are printed
    terminal_putchar('\n', FG_WHITE);
}

static void cmd_reboot(int argc, char* argv[]) {
    // void the args (they aren't used for rebooting)
    (void)argc;
    (void)argv;

    terminal_printf("Rebooting System...\n", FG_MAGENTA);

    // Wait for keyboard controller's input buffer to be clear
    // The second bit (bit 1) of the status port (0x64) is the input buffer status
    // We loop until it's 0 (clear)
    uint8_t status = inb(0x64);
    while(status & 0b10) {
        status = inb(0x64);
    }

    // Step 2: We send the pulse reset line command (0xFE) to the command port
    outb(0x64, 0xFE);

    // If the reboot command succeeded, the code will never reach here
    // however if it fails for whatever reason (e.g. on weird hardware or an emulator),
    // We should not continue. We'll halt the CPU in an infinite loop.
    terminal_printf("Reboot failed. System halted.\n", FG_RED);
    while(1) {
        asm volatile("hlt");
    }
}

static void cmd_memmap(int argc, char* argv[]) {
    (void)argc; // Unused
    (void)argv; // Unused

    uint32_t total_pages = pmm_get_total_pages();
    uint32_t used_pages = pmm_get_used_pages();
    uint32_t free_pages = total_pages - used_pages;

    // Each page is 4KB, so we can calculate memory in MB
    // (pages * 4) / 1024 = MB
    uint32_t total_mb = total_pages * 4 / 1024;
    uint32_t used_mb = used_pages * 4 / 1024;
    uint32_t free_mb = free_pages * 4 / 1024;

    terminal_printf("Physical Memory Usage:\n", FG_MAGENTA);
    terminal_printf("  Total: %d pages (%d MB)\n", FG_WHITE, total_pages, total_mb);
    terminal_printf("  Used:  %d pages (%d MB)\n", FG_RED, used_pages, used_mb);
    terminal_printf("  Free:  %d pages (%d MB)\n", FG_GREEN, free_pages, free_mb);
    terminal_printf("\nMemory Map (1 char = 512KB | 128 pages):\n", FG_MAGENTA);

    int pages_per_char = 128; // 1MB worth of 4KB pages

    for (uint32_t i = 0; i < total_pages / pages_per_char; i++) {
        uint32_t chunk_start_page = i * pages_per_char;
        uint32_t chunk_used_count = 0;

        // Count how many pages are used in this 1MB chunk
        for (int j = 0; j < pages_per_char; j++) {
            if (pmm_test_page(chunk_start_page + j)) {
                chunk_used_count++;
            }
        }

        // Print a character with a color based on usage
        if (chunk_used_count == 0) {
            terminal_printf(".", FG_LIGHT_GRAY); // Grey for free
        } else if (chunk_used_count < pages_per_char / 2) {
            terminal_printf("P",FG_GREEN); // Light green for partially used
        } else if (chunk_used_count < pages_per_char) {
            terminal_printf("M", FG_YELLOW); // Yellow for mostly used
        } else {
            terminal_printf("U", FG_RED); // Red for fully used
        }

        // Print a newline every 64MB for readability
        if ((i + 1) % 64 == 0) {
            terminal_putchar('\n', FG_WHITE);
        }
    }
    terminal_putchar('\n', FG_WHITE);
}

static void cmd_clear(int argc, char* argv[]) {
    (void)argc; // Mark as unused
    (void)argv; // Mark as unused
    terminal_initialize();
}

static void cmd_peek(int argc, char* argv[]) {
    if(argc < 2) {
        terminal_printf("Usage: peek <address>\n", FG_RED);
        return;
    }

    uint32_t address = hex_to_int(argv[1]);
    uint32_t* ptr = (uint32_t*)address;
    uint32_t value = *ptr;

    terminal_printf("Value at 0x%x: 0x%x\n", FG_MAGENTA, address, value);
}

static void cmd_poke(int argc, char* argv[]) {
    if(argc < 3) {
        terminal_printf("Usage: poke <address> <value>\n", FG_RED);
        return;
    }

    uint32_t address = hex_to_int(argv[1]);
    uint32_t value = hex_to_int(argv[2]);

    uint32_t* ptr = (uint32_t*)address;
    *ptr = value;   // Write the value to the address

    terminal_printf("Wrote 0x%x to address 0x%x\n", FG_MAGENTA, value, address);
}

// Command History definition
#define HISTORY_MAX_SIZE 16 // Store the last 16 commands

static char history_buffer[HISTORY_MAX_SIZE][CMD_BUFFER_SIZE];
static int history_count = 0;   // Amount of commands in history
static int history_head = 0;    // Index of the newest command
static int history_current = -1; // The index of the command we are currently viewing (-1 means none)

static void shell_history_add(const char* command) {
    if(strlen(command) == 0) return; // Don't save empty commands

    // Copy the command into the buffer at the current head position
    strcpy(history_buffer[history_head], command);

    // Move the head to the next slot, wrapping around if necessary
    history_head = (history_head + 1) % HISTORY_MAX_SIZE;

    // Increase the count, but don't let it exceed the max size
    if (history_count < HISTORY_MAX_SIZE) {
        history_count++;
    }

    // Reset the "current view" ptr whenever a new command is added
    history_current = -1;
}

// Redraws the current command line
static void shell_redraw_line(void) {
    // 1. Move cursor to the start of the line (after the prompt)
    for (size_t i = 0; i < buffer_index; i++) {
        terminal_putchar('\b', FG_WHITE);
    }

    // 2. Clear the old line by printing spaces
    for (size_t i = 0; i < buffer_index; i++) {
        terminal_putchar(' ', FG_WHITE);
    }

    // 3. Move cursor back to the start again
    for (size_t i = 0; i < buffer_index; i++) {
        terminal_putchar('\b', FG_WHITE);
    }

    // 4. Copy the new command into our buffer and print it
        if (history_current == -1) {
        // If we're not in history mode, just clear the buffer.
        cmd_buffer[0] = '\0';
    } else {
        // Otherwise, copy the command from history.
        strcpy(cmd_buffer, history_buffer[history_current]);
    }

    // Update the buffer index and print the new line.
    buffer_index = strlen(cmd_buffer);
    terminal_writestring(cmd_buffer, FG_WHITE);;
}

void shell_init(void) {
    buffer_index = 0;
    terminal_writestring(PROMPT, FG_MAGENTA); // Inital prompt
}

void shell_handle_key(int c) {
        if (c == KEY_UP) {
             terminal_printf("[DEBUG: history_count = %d, history_head = %d]\n", history_count, history_head);
        if (history_count > 0) {
            if (history_current == -1) {
                // If we are not in history mode, start from the newest command
                history_current = (history_head - 1 + HISTORY_MAX_SIZE) % HISTORY_MAX_SIZE;
            } else if (history_current != history_head) {
                // Move to the previous (older) command
                history_current = (history_current - 1 + HISTORY_MAX_SIZE) % HISTORY_MAX_SIZE;
            }
            shell_redraw_line();
        }
        return;
    }

    if (c == KEY_DOWN) {
        if (history_current != -1) {
            // Move to the next (newer) command
            history_current = (history_current + 1) % HISTORY_MAX_SIZE;
            if (history_current == history_head) {
                // We've reached the end, go back to a blank line
                history_current = -1;
                cmd_buffer[0] = '\0'; // Clear buffer
                shell_redraw_line(); // Redraw (which will clear the line)
            } else {
                shell_redraw_line();
            }
        }
        return;
    }

    // Handle backspace
    if (c == '\b') {
        if (buffer_index > 0) {
            buffer_index--;
            terminal_putchar('\b', FG_WHITE);
        }
        return;
    }

    // Handle Enter key
    if (c == '\n') {
        cmd_buffer[buffer_index] = '\0'; // Null-terminate the command
        terminal_putchar('\n', FG_WHITE);

        shell_history_add(cmd_buffer);

        process_command(cmd_buffer);

        buffer_index = 0;
        terminal_writestring("LxcidOS > ", FG_MAGENTA);
        return;
    }

    // Handle all other characters
    if (buffer_index < CMD_BUFFER_SIZE - 1) {
        cmd_buffer[buffer_index++] = c;
        terminal_putchar(c, FG_WHITE);    // Echo char to screen
    }
}
