#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h> // For size_t
#include <stdint.h>
#include "../lib/string.h"
#include "../io/io.h" // Add this include for outb

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

/* Function to initialize the terminal */
void terminal_initialize(void);

void terminal_clear();

void terminal_set_cursor(size_t x, size_t y);

/* Function to print a single character */
void terminal_putchar(char c, uint8_t color);

void terminal_writedec(uint32_t n);

/* Function to print a null-terminated string */
void terminal_writestring(const char* data, uint8_t color);

void terminal_printf(const char* format, uint8_t color, ...);

void terminal_writeerror(const char* format, ...);

void terminal_welcome();

#endif
