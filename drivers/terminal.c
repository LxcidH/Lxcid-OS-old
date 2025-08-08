#include <stdint.h>
#include "terminal.h"


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
    terminal_color = 0x0A; // light green text on black background

    // Clear the entire screen
    for(size_t y = 0; y < VGA_HEIGHT; y++) {
        for(size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            VGA_MEMORY[index] = vga_entry(' ', terminal_color);
        }
    }
}

// Puts a single character on the screen at the current cursor pos
void terminal_putchar(char c) {
    // Handle newlines seperately
    if(c == '\n'){
        terminal_column = 0;
        terminal_row++;
        // TODO: Add scrolling when terminal_row == VGA_HEIGHT
        return;
    }

    // Place the character at the current cursor pos
    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    VGA_MEMORY[index] = vga_entry(c, terminal_color);

    // Advance the cursor
    if(++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        terminal_row++;
        // TODO: Add scrolling when terminal_row == VGA_HEIGHT
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
