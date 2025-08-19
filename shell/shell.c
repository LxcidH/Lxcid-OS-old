#include "shell.h"
#include "../lib/string.h"
#include "../io/io.h"
#include "../drivers/keyboard.h"
#include "../fs/fat32.h"
#include "../lib/math.h"
#include "../memory/pmm.h"
#include "../memory/heap.h"

jmp_buf g_shell_checkpoint;
uint32_t g_current_directory_cluster;

// Forward declarations for command handlers (static cus they're internal)
static void cmd_help(int argc, char* argv[]);
static void cmd_echo(int argc, char* argv[]);
static void cmd_reboot(int argc, char* argv[]);
static void cmd_memmap(int argc, char* argv[]);
static void cmd_clear(int argc, char* argv[]);
static void cmd_peek(int argc, char* argv[]);
static void cmd_poke(int argc, char* argv[]);
static void cmd_mkfile(int argc, char* argv[]);
static void cmd_mkdir(int argc, char* argv[]);
static void cmd_ls(int argc, char* argv[]);
static void cmd_rm(int argc, char* argv[]);
static void cmd_cd(int argc, char* argv[]);
static void cmd_run(int argc, char* argv[]);

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
    {"poke", cmd_poke, "Writes a 32-bit value to a memory address.\n"},
    {"ls", cmd_ls, "Lists the files within the current directory.\n"},
    {"mkfile", cmd_mkfile, "Creates a file in the current directory with the defined filename.\n"},
    {"mkdir", cmd_mkdir, "Creates a directory at the specified location.\n"},
    {"rm", cmd_rm, "Removes a file/directory.\n"},
    {"cd", cmd_cd, "Changes directory to the specified path!\n"},
    {"run", cmd_run, "Runs a binary file!\n"}
};
static const int num_commands = sizeof(commands) / sizeof(shell_command_t);

#define CMD_BUFFER_SIZE 256
#define PROMPT "LxcidOS > "
#define MAX_ARGS 16

static char cmd_buffer[CMD_BUFFER_SIZE];
static size_t buffer_index = 0;
static size_t cursor_pos = 0;
static size_t last_buffer_len = 0;

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
    uint32_t heap_mb = HEAP_SIZE_PAGES * 4 / 1024;

    terminal_printf("Physical Memory Usage:\n", FG_MAGENTA);
    terminal_printf("  Total: %d pages (%d MB)\n", FG_WHITE, total_pages, total_mb);
    terminal_printf("  Used:  %d pages (%d MB)\n", FG_RED, used_pages, used_mb);
    terminal_printf("  Free:  %d pages (%d MB)\n", FG_GREEN, free_pages, free_mb);
    terminal_printf("  Heap:  %d pages (%d MB)\n", FG_GREEN, HEAP_SIZE_PAGES, heap_mb);
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

    terminal_printf("Value at %x: %x\n", FG_MAGENTA, address, value);
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

static void cmd_ls(int argc, char* argv[]) {
    // This function now ignores argc and argv for simplicity
    fat32_list_dir(g_current_directory_cluster);
}

static void cmd_mkfile(int argc, char* argv[]) {
    if (argc < 2) {
        terminal_printf("USAGE: mkfile <filename.extension>\n", FG_RED);
        return;
    }
    if(fat32_create_file(argv[1], g_current_directory_cluster)) {
        terminal_printf("%s created!\n", FG_GREEN, argv[1]);
    } else {
        terminal_writeerror("%s couldn't be created!\n", argv[1]);
    }
}

static void cmd_mkdir(int argc, char* argv[]) {
    if (argc < 2) {
        terminal_printf("USAGE: mkdir <dirname>\n", FG_RED);
        return;
    }
    if (fat32_create_directory(argv[1], g_current_directory_cluster)) {
        terminal_printf("%s created successfully!\n", FG_GREEN, argv[1]);
    } else {
        terminal_writeerror("%s couldn't be created!\n", argv[1]);
    }
}

static void cmd_rm(int argc, char* argv[]) {
    if (argc < 2) {
        terminal_printf("USAGE: rm <filename.extension>\n", FG_RED);
        terminal_printf("USAGE 2: rm -rf <dirname>\n", FG_RED);
        return;
    }
    if (argc == 2) {
        if (fat32_delete_file(argv[1], g_current_directory_cluster)) {
            terminal_printf("%s deleted!\n", FG_GREEN, argv[1]);
        } else {
            terminal_writeerror("File couldn't be deleted!\n");
        }
    }
    if (argc > 2 && strcmp(argv[1], "-rf") == 0) {
        if (fat32_delete_directory(argv[2], g_current_directory_cluster)) {
            terminal_printf("<DIR> '%s' was deleted!\n", FG_GREEN, argv[2]);
        }
    }
}

static void cmd_cd(int argc, char* argv[]) {
    if (argc < 2) {
        g_current_directory_cluster = fat32_get_root_cluster();
        return;
    }

    const char* dirname = argv[1];
    FAT32_DirectoryEntry* entry = fat32_find_entry(dirname, g_current_directory_cluster);

    if (entry == NULL) {
        terminal_printf("Error: Directory '%s' not found.\n", FG_RED, dirname);
        return;
    }
    if (!(entry->attr & ATTR_DIRECTORY)) {
        terminal_printf("Error: '%s' is not a directory.\n", FG_RED, dirname);
        return;
    }

    uint32_t new_cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;

    // The root's parent is cluster 0, so we reset to the actual root cluster
    if (new_cluster == 0) {
        g_current_directory_cluster = fat32_get_root_cluster();
    } else {
        g_current_directory_cluster = new_cluster;
    }
}

void cmd_run(int argc, char* argv[]) {
    if (argc < 2) {
        terminal_printf("USAGE: run <program.bin>\n", FG_RED);
        return;
    }

    FAT32_DirectoryEntry* program = fat32_find_entry(argv[1], g_current_directory_cluster);
    if (program == NULL) {
        terminal_printf("Error: Program '%s' not found.\n", FG_RED, argv[1]);
        return;
    }

    // --- THIS IS THE FIX ---
    // Allocate space for the file's content PLUS ONE for the null terminator.
    uint8_t* pMemory = 0x150000;
    // -------------------------
    if (pMemory == NULL) {
        terminal_writeerror("No available memory!\n");
        return;
    }

    // Read the file into the allocated memory
    fat32_read_file(program, pMemory);

    // Now it is safe to add the null terminator
    pMemory[program->file_size] = '\0';

    // Create a function pointer and execute
    void (*program_entry)(void) = (void (*)())pMemory;

    if (setjmp(g_shell_checkpoint) == 0) {
        terminal_printf("Executing '%s'...\n", FG_MAGENTA, argv[1]);
        program_entry();
    } else {
        free(pMemory);
    }
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
static void shell_redraw_line() {
    size_t prompt_len = strlen(PROMPT);
    size_t start_row = terminal_get_row();

    // 1. Move cursor to the start of the command
    terminal_set_cursor(prompt_len, start_row);

    // 2. Write the current buffer
    terminal_writestring(cmd_buffer, FG_WHITE);

    // 3. If the new buffer is SHORTER than the last one, clear the difference
    if (buffer_index < last_buffer_len) {
        for (size_t i = 0; i < last_buffer_len - buffer_index; i++) {
            terminal_putchar(' ', FG_WHITE);
        }
    }

    // 4. Update the last known length for the next redraw
    last_buffer_len = buffer_index;

    // 5. Move the hardware cursor to the correct final position
    terminal_set_cursor(prompt_len + cursor_pos, start_row);
}


void shell_init(void) {
    buffer_index = 0;
    g_current_directory_cluster = fat32_get_root_cluster();
    terminal_printf("%s", FG_MAGENTA, PROMPT); // Inital prompt
}

void shell_handle_key(int key) {
    switch (key) {
        case KEY_UP:
            // PASTE YOUR EXISTING 'KEY_UP' LOGIC FOR COMMAND HISTORY HERE.
            // After you load a command from history, you must update the state:
            // buffer_index = strlen(cmd_buffer);
            // cursor_pos = buffer_index;
            // Then call shell_redraw_line();
            break;

        case KEY_DOWN:
            // PASTE YOUR EXISTING 'KEY_DOWN' LOGIC FOR COMMAND HISTORY HERE.
            // After you load a command from history, you must update the state:
            // buffer_index = strlen(cmd_buffer);
            // cursor_pos = buffer_index;
            // Then call shell_redraw_line();
            break;

        case KEY_LEFT:
            if (cursor_pos > 0) {
                cursor_pos--;
                terminal_set_cursor(strlen(PROMPT) + cursor_pos, terminal_get_row());
            }
            break;

        case KEY_RIGHT:
            if (cursor_pos < buffer_index) {
                cursor_pos++;
                terminal_set_cursor(strlen(PROMPT) + cursor_pos, terminal_get_row());
            }
            break;

        case '\b': // Backspace
            if (cursor_pos > 0) {
                // Shift buffer contents from the cursor position left by one
                memmove(&cmd_buffer[cursor_pos - 1], &cmd_buffer[cursor_pos], buffer_index - cursor_pos);
                buffer_index--;
                cursor_pos--;
                cmd_buffer[buffer_index] = '\0'; // Re-apply null terminator
                shell_redraw_line();
            }
            break;

        case '\n': // Enter
            cmd_buffer[buffer_index] = '\0';
            terminal_putchar('\n', FG_WHITE);

            if (buffer_index > 0) {
                shell_history_add(cmd_buffer);
                process_command(cmd_buffer);
            }

            // Reset state for the next command
            buffer_index = 0;
            cursor_pos = 0;
            last_buffer_len = 0; // ADD THIS LINE
            cmd_buffer[0] = '\0';
            terminal_printf(PROMPT, FG_MAGENTA);
            break; // Use break if you changed from 'return'

        default: // Printable character
            if (key >= 32 && key <= 126 && buffer_index < CMD_BUFFER_SIZE - 1) {
                // Shift buffer contents from the cursor position right by one to make space
                memmove(&cmd_buffer[cursor_pos + 1], &cmd_buffer[cursor_pos], buffer_index - cursor_pos);

                // Insert the new character
                cmd_buffer[cursor_pos] = (char)key;
                buffer_index++;
                cursor_pos++;
                cmd_buffer[buffer_index] = '\0'; // Re-apply null terminator
                shell_redraw_line();
            }
            break;
    }
}
