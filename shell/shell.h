#ifndef SHELL_H
#define SHELL_H

#include "../drivers/terminal.h"
#include <stddef.h>
#include "../lib/setjmp.h"

extern jmp_buf g_shell_checkpoint;
extern jmp_buf g_shell_checkpoint;
extern uint32_t g_current_directory_cluster;
// Initializes the shell
void shell_init(void);

// Handles a key press from the keyboard driver
void shell_handle_key(int c);

#endif
