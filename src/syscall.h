#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>
#include "../idt/idt.h"

#define SYS_EXIT  1
#define SYS_WRITE 4
#define SYS_READ  3 // We will need this later
#define SYS_OPEN  5 // And this one too

void syscall_handler(registers_t* regs);
#endif
