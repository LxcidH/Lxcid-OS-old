#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h> // For size_t
#include <stdint.h>

/* Function to initialize the terminal */
void terminal_initialize(void);

void terminal_set_cursor(size_t x, size_t y);

/* Function to print a single character */
void terminal_putchar(char c);

void terminal_writedec(uint32_t n);

/* Function to print a null-terminated string */
void terminal_writestring(const char* data);

void terminal_writeerror(const char* data);

void terminal_welcome();

#endif
