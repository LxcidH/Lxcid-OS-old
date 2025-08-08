#ifndef PIC_H
#define PIC_H

#include <stdint.h>

// Function to remap the PIC
void pic_remap(void);

// Function to send an End-of-Interrupt (EOI) signal
void pic_send_eoi(uint8_t irq);

#endif
