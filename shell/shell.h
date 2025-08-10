#ifndef SHELL_H
#define SHELL_H

#include "../drivers/terminal.h"
#include <stddef.h>

// Initializes the shell
void shell_init(void);

// Handles a key press from the keyboard driver
void shell_handle_key(int c);

#endif
