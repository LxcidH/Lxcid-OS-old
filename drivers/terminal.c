#include "terminal.h"
#include <stdarg.h>
#include "../io/io.h" // Required for outb()

// Define the dimensions of the VGA text-mode buffer
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

// The video memory buffer starts at 0xB8000
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

// Variables to keep track of the cursor position and color
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;

// Creates a VGA entry from a character and a color
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

/**
 * @brief Sets the hardware cursor's position using VGA I/O ports.
 */
void terminal_set_cursor(size_t x, size_t y) {
    uint16_t pos = y * VGA_WIDTH + x;

    // Send the high byte of the position to register 0x0E
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));

    // Send the low byte of the position to register 0x0F
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
}

// Initialises the terminal by clearing it and setting up the state
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = FG_WHITE;
    terminal_clear();
    // Set the hardware cursor to the starting position
    terminal_set_cursor(terminal_column, terminal_row);
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
    }
    // Handle newline character
    else if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
    }
    // Handle all other, normal characters
    else {
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

    // After ANY action, update the hardware cursor's physical position
    terminal_set_cursor(terminal_column, terminal_row);

    terminal_color = orig_term_color;
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
                terminal_writestring(s, color);
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
                terminal_writestring("0x", color);
                itoa(x, int_buffer, 16);
                terminal_writestring(int_buffer, color);
                break;
            }
        }
    }
}

// This is now just a convenient wrapper around terminal_vprintf.
void terminal_printf(const char* format, uint8_t color, ...) {
    va_list args;
    va_start(args, color);
    terminal_vprintf(format, color, args);
    va_end(args);
}

// Writes a null-terminated string to the terminal
void terminal_writestring(const char* data, uint8_t color) {
    size_t len = 0;
    while(data[len]) {
        terminal_putchar(data[len], color);
        len++;
    }
}

void terminal_writeerror(const char* format, ...) {
    terminal_printf("ERROR: ", FG_RED);
    va_list args;
    va_start(args, format);
    terminal_vprintf(format, FG_RED, args);
    va_end(args);
    terminal_putchar('\n', FG_RED);
}

void terminal_welcome() {
    terminal_printf("LxcidOS - Version 0.0.1\n", FG_MAGENTA);
}

// The terminal_writedec function is not used by the rest of your code
// and can be safely removed if you wish. It remains here for completeness.
void terminal_writedec(uint32_t n) {
    if (n == 0) {
        terminal_putchar('0', FG_WHITE);
        terminal_writestring("         ", FG_WHITE);
        return;
    }
    char buffer[12];
    int i = 0;
    while (n > 0) {
        buffer[i] = (n % 10) + '0';
        n /= 10;
        i++;
    }
    for (int j = i - 1; j >= 0; j--) {
        terminal_putchar(buffer[j], FG_WHITE);
    }
    for (int k = i; k < 10; k++) {
        terminal_putchar(' ', FG_WHITE);
    }
}
