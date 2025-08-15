#ifndef SETJMP_H
#define SETJMP_H

#include <stdint.h>

// A simple jmp_buf for i386. It stores the registers we need to restore.
typedef struct {
    uint32_t ebx, esp, ebp, esi, edi, eip;
} jmp_buf[1];

// Function Prototypes
int setjmp(jmp_buf buf);
void longjmp(jmp_buf buf, int val);

#endif
