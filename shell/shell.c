#include "shell.h"
#include "../lib/string.h"
#include "../io/io.h"
#include "../drivers/keyboard.h"
#include "../fs/fat32.h"
#include "../lib/math.h"
#include "../memory/pmm.h"
#include "../memory/heap.h"
#include "../fs/elf.h"

jmp_buf g_shell_checkpoint;
uint32_t g_current_directory_cluster;
#define MAX_PATH_LENGTH 256
char g_current_path[MAX_PATH_LENGTH] = "root"; // Start at the root;
// Forward declarations for command handlers (static cus they're internal)
static void cmd_help(int argc, char* argv[]);
static void cmd_echo(int argc, char* argv[]);
static void cmd_reboot(int argc, char* argv[]);
static void cmd_memmap(int argc, char* argv[]);
static void cmd_clear(int argc, char* argv[]);
static void cmd_peek(int argc, char* argv[]);
static void cmd_poke(int argc, char* argv[]);
static void cmd_touch(int argc, char* argv[]);
static void cmd_mkdir(int argc, char* argv[]);
static void cmd_ls(int argc, char* argv[]);
static void cmd_rm(int argc, char* argv[]);
static void cmd_cd(int argc, char* argv[]);
static void cmd_cp(int argc, char* argv[]);
static void cmd_run(int argc, char* argv[]);
static void cmd_dInfo(int argc, char* argv[]);
static void cmd_fwrite(int argc, char* argv[]);
static void cmd_cat(int argc, char* argv[]);

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
    {"touch", cmd_touch, "Creates a file in the current directory with the defined filename.\n"},
    {"mkdir", cmd_mkdir, "Creates a directory at the specified location.\n"},
    {"rm", cmd_rm, "Removes a file/directory.\n"},
    {"cd", cmd_cd, "Changes directory to the specified path!\n"},
    {"cp", cmd_cp, "Copies a file to another path.\n"},
    {"run", cmd_run, "Runs a binary file!\n"},
    {"dInfo", cmd_dInfo, "Shows info of all attached drives\n"},
    {"fwrite", cmd_fwrite, "Writes a buffer to the specified file\n"},
    {"cat", cmd_cat, "Reads a file to the terminal\n"}
};
static const int num_commands = sizeof(commands) / sizeof(shell_command_t);

#define CMD_BUFFER_SIZE 256
#define PROMPT "LxcidOS > "
#define MAX_ARGS 16

#define MAX_CMD_LEN 256
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_len = 0;

static char cmd_buffer[CMD_BUFFER_SIZE];
static size_t buffer_index = 0;
static size_t cursor_pos = 0;
static size_t last_buffer_index = 0;

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
        terminal_printf("  %s - %s", FG_WHITE, commands[i].name, commands[i].help_text);
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
  terminal_printf("-------------------------------- LxcidOS v1.0.0 --------------------------------\n", FG_MAGENTA);
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

static void cmd_touch(int argc, char* argv[]) {
    if (argc < 2) {
        terminal_printf("USAGE: touch <filename.extension>\n", FG_RED);
        return;
    }dir_entry_location_t new_file_loc;
    if(fat32_create_file(argv[1], g_current_directory_cluster, &new_file_loc)) {
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
        strcpy(g_current_path, "/"); // Reset path to root
        return;
    }

    const char* dirname = argv[1];
    // --- HANDLE ".." FIRST ---
    if (strcmp(dirname, "..") == 0) {
        // If we are already at the root, do nothing.
        if (g_current_directory_cluster == fat32_get_root_cluster()) {
            return;
        }

        // Get the parent directory's cluster
        uint32_t parent_cluster = fat32_get_parent_cluster(g_current_directory_cluster);
        if (parent_cluster != g_current_directory_cluster) {
            g_current_directory_cluster = parent_cluster;

            // Find the last slash to truncate the path string
            char* last_slash = strrchr(g_current_path, '/');
            if (last_slash) {
                *last_slash = '\0';
                if (strlen(g_current_path) == 0) {
                    strcpy(g_current_path, "/");
                }
            }
        }
        return;
    }

    // --- THEN HANDLE NORMAL DIRECTORY CHANGES ---
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

    // Check if the directory is already the current one
    if (new_cluster == g_current_directory_cluster) {
        return;
    }

    g_current_directory_cluster = new_cluster;

    // Append the new directory name to the path
    if (strcmp(g_current_path, "/") != 0) {
        strcat(g_current_path, "/");
    }
    strcat(g_current_path, dirname);
    return;
}

static void cmd_cp(int argc, char* argv[]) {
    if (argc < 3) {
        terminal_printf("USAGE: cp <source> <dest>\n", FG_RED);
        return;
    }

    // 1. Find the source file entry.
    FAT32_DirectoryEntry* source_entry = fat32_find_entry(argv[1], g_current_directory_cluster);
    if (source_entry == NULL) {
        terminal_printf("Error: Source file '%s' not found.\n", FG_RED, argv[1]);
        return;
    }

    // 2. Read the source file's content into a buffer.
    void* buffer = malloc(source_entry->file_size);
    if (buffer == NULL) {
        terminal_printf("Error: Memory allocation failed.\n", FG_RED);
        return;
    }
    fat32_read_file(source_entry, buffer);

    // 3. Create the destination file on disk and get its location.
    dir_entry_location_t dest_loc;
    if (!fat32_create_file(argv[2], g_current_directory_cluster, &dest_loc)) {
        free(buffer);
        return;
    }

    // 4. Create a LOCAL directory entry struct on the stack.
    FAT32_DirectoryEntry dest_entry_struct;

    // 5. Find the newly created entry on disk and copy its data into our local struct.
    // We use the more detailed `fat32_find_entry_by_name` here.
    if (!fat32_find_entry_by_name(argv[2], g_current_directory_cluster, NULL, &dest_entry_struct)) {
        terminal_printf("Error: Could not find newly created destination file.\n", FG_RED);
        free(buffer);
        return;
    }

    // 6. Write the data to the file. This will modify our LOCAL struct (`dest_entry_struct`)
    // with the new file size and starting cluster.
    bool success = fat32_write_file(&dest_entry_struct, buffer, source_entry->file_size);

    // 7. CRITICAL STEP: Write the updated LOCAL struct back to the disk at the correct location.
    if (success) {
        if (fat32_update_entry(&dest_entry_struct, &dest_loc)) {
            terminal_printf("File copied successfully.\n", FG_GREEN);
        } else {
            terminal_printf("Error: Failed to update directory entry on disk.\n", FG_RED);
        }
    } else {
        terminal_printf("Error: Failed to write file data.\n", FG_RED);
    }

    // 8. Clean up the buffer.
    free(buffer);
}

void cmd_run(int argc, char* argv[]) {
  if (argc < 2) {
        terminal_printf("USAGE: run <program.elf>\n", FG_RED);
        return;
    }

    FAT32_DirectoryEntry* program_entry = fat32_find_entry(argv[1], g_current_directory_cluster);
    if (program_entry == NULL) {
        terminal_printf("Error: Program '%s' not found.\n", FG_RED, argv[1]);
        return;
    }

    terminal_printf("Executing '%s'...\n", FG_MAGENTA, argv[1]);

    uint32_t entry_point = elf_load(program_entry);

    if (entry_point == 0) {
        terminal_printf("Failed to execute program (ELF loading error).\n", FG_RED);
        return;
    }

    void (*program_start)(void) = (void (*)(void))entry_point;

    if (setjmp(g_shell_checkpoint) == 0) {
        program_start();
    } else {
        terminal_printf("\nProgram finished, returning to shell.\n", FG_GREEN);
    }
}

void cmd_dInfo(int argc, char* argv[]) {
    disk_info disk_info = fat32_get_disk_size();
    terminal_printf("| Volume ID | Volume Label | Volume Size (Bytes) |\n", FG_MAGENTA);
    terminal_printf("| %d | %s | %d |\n", FG_MAGENTA, disk_info.vol_id, disk_info.vol_lab, disk_info.disk_size_bytes, disk_info.used_space);
    return;
}

void cmd_fwrite(int argc, char* argv[]) {
    if (argc < 3) { // 1. Must be 3 arguments
        terminal_printf("USAGE: fwrite <filename> <text>\n", FG_MAGENTA);
        return;
    }

    FAT32_DirectoryEntry entry;
    dir_entry_location_t loc;
    
    // Find the file and get its location
    if (!fat32_find_entry_by_name(argv[1], g_current_directory_cluster, &loc, &entry)) {
        terminal_printf("ERROR: Failed to find %s\n", FG_RED, argv[1]);
        return;
    }
    
    // Get the data and size to write
    const char* buffer = argv[2];
    uint32_t size = strlen(buffer);

    // 2. Write the file data
    if (fat32_write_file(&entry, buffer, size)) {
        // 3. CRITICAL: Write the updated entry (with new size/cluster) back to disk
        if (fat32_update_entry(&entry, &loc)) {
            terminal_printf("Wrote %d bytes to %s\n", FG_GREEN, size, argv[1]);
        } else {
            terminal_printf("ERROR: Failed to update directory entry.\n", FG_RED);
        }
    } else {
        terminal_printf("ERROR: Failed to write file data.\n", FG_RED);
    }
    
    // 4. No free needed because 'entry' and 'loc' are on the stack
}
void cmd_cat(int argc, char* argv[]) {
    if (argc < 2) {
        terminal_printf("USAGE: cat <filename>\n", FG_MAGENTA);
        return;
    }

    // 1. Get the file entry (this is heap-allocated)
    FAT32_DirectoryEntry* file = fat32_find_entry(argv[1], g_current_directory_cluster);

    if (file == NULL) {
        terminal_printf("ERROR: Failed to find %s\n", FG_RED, argv[1]);
        return; // 'file' is already NULL, so no free needed
    }

    if (file->file_size == 0) {
        // Handle empty file, nothing to read
        free(file); // Don't forget to free!
        return;
    }

    // 2. Allocate the *correct* buffer size (+1 for null terminator)
    char* buffer = malloc(file->file_size + 1);
    if (buffer == NULL) {
        terminal_printf("ERROR: Not enough memory to read file.\n", FG_RED);
        free(file); // Free the file entry before returning
        return;
    }

    // 3. Pass 'buffer' (char*), NOT '&buffer' (char**)
    fat32_read_file(file, buffer);

    // 4. Null-terminate the buffer so we can print it as a string
    buffer[file->file_size] = '\0';

    // 5. Actually print the content
    terminal_printf("%s\n", FG_WHITE, buffer);

    // 6. Free ALL allocated memory
    free(buffer);
    free(file);
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
    size_t prompt_len = strlen("LxcidOS |  >") + strlen(g_current_path);
    size_t start_row = terminal_get_row();

    // 1. Move cursor to the start of the command
    terminal_set_cursor(prompt_len, start_row);

    // 2. Write the current buffer
    terminal_writestring(cmd_buffer, FG_WHITE);

    // 3. If the new buffer is SHORTER than the last one, clear the difference
    if (buffer_index < last_buffer_index) {
        for (size_t i = 0; i < last_buffer_index - buffer_index; i++) {
            terminal_putchar(' ', FG_WHITE);
        }
    }

    // 4. Update the last known length for the next redraw
    last_buffer_index = buffer_index;

    // 5. Move the hardware cursor to the correct final position
    terminal_set_cursor(prompt_len + cursor_pos, start_row);
}

void shell_init(void) {
    buffer_index = 0;
    g_current_directory_cluster = fat32_get_root_cluster();
    terminal_printf("-------------------------------- LxcidOS v1.0.0 --------------------------------\n", FG_MAGENTA);
    terminal_printf("LxcidOS | %s >", FG_MAGENTA, g_current_path); // Inital prompt
}

void shell_handle_key(int key) {
    switch (key) {
        case KEY_UP:
            if (history_count > 0) {
                if (history_current == -1) {
                    // If not in history mode, start with the newest command
                    history_current = (history_head - 1 + HISTORY_MAX_SIZE) % HISTORY_MAX_SIZE;
                } else {
                    // Move to the previous (older) command
                    int oldest_index = (history_head - history_count + HISTORY_MAX_SIZE) % HISTORY_MAX_SIZE;
                    if (history_current != oldest_index) {
                        history_current = (history_current - 1 + HISTORY_MAX_SIZE) % HISTORY_MAX_SIZE;
                    }
                }
                // Copy history to buffer and update state
                strcpy(cmd_buffer, history_buffer[history_current]);
                buffer_index = strlen(cmd_buffer);
                cursor_pos = buffer_index;
                shell_redraw_line();
            }
            break;

        case KEY_DOWN:
            if (history_current != -1) {
                // Move to the next (newer) command
                history_current = (history_current + 1) % HISTORY_MAX_SIZE;
                if (history_current == history_head) {
                    // Reached the end, exit history mode
                    history_current = -1;
                    cmd_buffer[0] = '\0';
                } else {
                    strcpy(cmd_buffer, history_buffer[history_current]);
                }
                // Update state and redraw
                buffer_index = strlen(cmd_buffer);
                cursor_pos = buffer_index;
                shell_redraw_line();
            }
            break;

        case KEY_LEFT:
            if (cursor_pos > 0) {
                cursor_pos--;
                terminal_set_cursor(strlen("LxcidOS |  >") + strlen(g_current_path) + cursor_pos, terminal_get_row());
            }
            break;

        case KEY_RIGHT:
            if (cursor_pos < buffer_index) {
                cursor_pos++;
                terminal_set_cursor(strlen("LxcidOS |  >") + strlen(g_current_path) + cursor_pos, terminal_get_row());
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
            last_buffer_index = 0; // ADD THIS LINE
            cmd_buffer[0] = '\0';
            terminal_printf("LxcidOS | %s >", FG_MAGENTA, g_current_path);
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

uint32_t shell_getCurrentDirCluster(void) {
  return g_current_directory_cluster;
}
