#include "terminal.h"

// Define terminal colors
enum fgcolor {
    FG_BLACK         =   0x00,
    FG_BLUE          =   0x01,
    FG_GREEN         =   0x02,
    FG_CYAN          =   0x03,
    FG_RED           =   0x04,
    FG_MAGENTA       =   0x05,
    FG_BROWN         =   0x06,
    FG_LIGHT_GRAY    =   0x07,
    FG_DARK_GRAY     =   0x08,
    FG_LIGHT_BLUE    =   0x09,
    FG_LIGHT_GREEN   =   0x0A,
    FG_LIGHT_CYAN    =   0x0B,
    FG_LIGHT_RED     =   0x0C,
    FG_LIGHT_MAGENTA =   0x0D,
    FG_YELLOW        =   0x0E,
    FG_WHITE         =   0x0F
};

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


// Variables to keep track of the cursor position and color
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;


// Initialises the terminal by clearing it and setting up the state
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = FG_WHITE; // light green text on black background

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
void terminal_putchar(char c) {
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
}

// A robust function to print a number in decimal.
// It also pads with spaces to clear old digits.
void terminal_writedec(uint32_t n) {
    // Handle the special case of 0 separately
    if (n == 0) {
        terminal_putchar('0');
        terminal_writestring("         "); // Pad with spaces
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
        terminal_putchar(buffer[j]);
    }

    // After printing the number, print enough spaces to clear out any
    // digits from a previous, larger number (e.g., clearing '10' when printing '9').
    // We'll pad up to 10 digits.
    for (int k = i; k < 10; k++) {
        terminal_putchar(' ');
    }
}

// Writes a null-terminated string to the terminal
void terminal_writestring(const char* data) {
    size_t len = 0;
    while(data[len]) {
        terminal_putchar(data[len]);
        len++;
    }
}

// Writes a null-terminated error msg to terminal
void terminal_writeerror(const char* data) {
    uint8_t orig_term_color = terminal_color;
    terminal_color = FG_RED;
    terminal_writestring("ERROR: ");
    terminal_writestring(data);
    terminal_color = orig_term_color;
}

void terminal_welcome() {
    terminal_color = FG_MAGENTA;
    terminal_writestring("LxcidOS - Version 0.0.1\n");
    terminal_color = FG_WHITE;
}
