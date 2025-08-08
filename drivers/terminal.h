#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h> // For size_t

/* Function to initialize the terminal */
void terminal_initialize(void);

/* Function to print a single character */
void terminal_putchar(char c);

/* Function to print a null-terminated string */
void terminal_writestring(const char* data);

#endif
