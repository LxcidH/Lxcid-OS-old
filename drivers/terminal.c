#include "terminal.h"
#include <stdarg.h>

// Define the dimensions of the VGA text-mode buffer
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

// The video memory buffer starts at 0xB8000
// We treat it as a pointer to uint16_t values
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

// Variables to keep track of the cursor position and color
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;

// Creates a VGA entry from a character and a color
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

// Initialises the terminal by clearing it and setting up the state
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = FG_WHITE;
    terminal_clear();
}

void terminal_clear() {
    // Clear the entire screen
    for(size_t y = 0; y < VGA_HEIGHT; y++) {
        for(size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            VGA_MEMORY[index] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_set_cursor(size_t x, size_t y) {
    terminal_column = x;
    terminal_row = y;
    // The VGA hardware cursor is controlled via I/O ports
    // We will add this later
}

// Scrolls the terminal
void terminal_scroll() {
    for(size_t y = 1; y < VGA_HEIGHT; y++) {
        for(size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index_to = (y - 1) * VGA_WIDTH + x;
            const size_t index_from = y * VGA_WIDTH + x;
            VGA_MEMORY[index_to] = VGA_MEMORY[index_from];
        }
    }
    // Clear last line of terminal
    const size_t last_row = VGA_HEIGHT - 1;
    for(size_t x = 0; x < VGA_WIDTH; x++) {
        const size_t index = last_row * VGA_WIDTH + x;
        VGA_MEMORY[index] = vga_entry(' ', terminal_color);
    }
    terminal_row = VGA_HEIGHT - 1;
}

// Puts a single character on the screen at the current cursor pos
void terminal_putchar(char c, uint8_t color) {
    uint8_t orig_term_color = terminal_color;
    terminal_color = color;
    // Handle backspace character
    if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            const size_t index = terminal_row * VGA_WIDTH + terminal_column;
            VGA_MEMORY[index] = vga_entry(' ', terminal_color);
        }
        // Note: This doesn't handle backspacing over a line break yet.
        return;
    }

    // Handle newline character
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
    } else {
        // Handle all other, normal characters
        const size_t index = terminal_row * VGA_WIDTH + terminal_column;
        VGA_MEMORY[index] = vga_entry(c, terminal_color);
        if (++terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            terminal_row++;
        }
    }

    // If the cursor is now off the screen, scroll.
    if (terminal_row >= VGA_HEIGHT) {
        terminal_scroll();
    }

    terminal_color = orig_term_color;
}

// A robust function to print a number in decimal.
// It also pads with spaces to clear old digits.
void terminal_writedec(uint32_t n) {
    // Handle the special case of 0 separately
    if (n == 0) {
        terminal_putchar('0', FG_WHITE);
        terminal_writestring("         ", FG_WHITE); // Pad with spaces
        return;
    }

    // A small buffer to hold the digits of the number
    char buffer[12];
    int i = 0;

    // Convert the number to a string of digits, in reverse order
    while (n > 0) {
        buffer[i] = (n % 10) + '0';
        n /= 10;
        i++;
    }

    // The digits are in reverse order, so we need to print the buffer backwards
    for (int j = i - 1; j >= 0; j--) {
        terminal_putchar(buffer[j], FG_WHITE);
    }

    // After printing the number, print enough spaces to clear out any
    // digits from a previous, larger number (e.g., clearing '10' when printing '9').
    // We'll pad up to 10 digits.
    for (int k = i; k < 10; k++) {
        terminal_putchar(' ', FG_WHITE);
    }
}

// This function does the actual work. It takes a va_list.
void terminal_vprintf(const char* format, uint8_t color, va_list args) {
    char int_buffer[32];

    for (const char* p = format; *p != '\0'; p++) {
        if (*p != '%') {
            terminal_putchar(*p, color);
            continue;
        }

        p++; // Move to the character after '%'

        switch (*p) {
            case 'c': {
                char c = (char)va_arg(args, int);
                terminal_putchar(c, color);
                break;
            }
            case 's': {
                const char* s = va_arg(args, const char*);
                terminal_writestring(s, color); // Assuming you have a color version
                break;
            }
            case 'd': {
                int d = va_arg(args, int);
                itoa(d, int_buffer, 10);
                terminal_writestring(int_buffer, color);
                break;
            }
            case 'x': {
                int x = va_arg(args, int);
                // It's common to add the "0x" prefix for clarity
                terminal_writestring("0x", color);
                itoa(x, int_buffer, 16); // Use base 16 for hexadecimal
                terminal_writestring(int_buffer, color);
            }
            // ... other cases ...
        }
    }
}

// This is now just a convenient wrapper around terminal_vprintf.
void terminal_printf(const char* format, uint8_t color, ...) {
    va_list args;
    va_start(args, color); // Initialize the list after the last named argument

    terminal_vprintf(format, color, args); // Call the real worker function

    va_end(args); // Clean up
}

// Writes a null-terminated string to the terminal
void terminal_writestring(const char* data, uint8_t color) {
    uint8_t orig_term_color = terminal_color;
    terminal_color = color;
    size_t len = 0;
    while(data[len]) {
        terminal_putchar(data[len], color);
        len++;
    }
    terminal_color = orig_term_color;
}

void terminal_writeerror(const char* format, ...) {
    // 1. Print the "ERROR: " prefix in red
    terminal_printf("ERROR: ", FG_RED);

    // 2. Process the user's format string and arguments
    va_list args;
    va_start(args, format);
    terminal_vprintf(format, FG_RED, args);
    va_end(args);

    // 3. Print a newline at the end
    terminal_putchar('\n', FG_RED);
}

void terminal_welcome() {
    terminal_printf("LxcidOS - Version 0.0.1\n", FG_MAGENTA);
}
