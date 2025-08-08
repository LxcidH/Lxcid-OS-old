#include "pic.h"
#include "../io/io.h"

// I/O port addresses for the two PIC chips
#define PIC1                    0x20    // Master PIC
#define PIC2                    0xA0    // Slave PIC
#define PIC1_COMMAND            PIC1
#define PIC1_DATA               (PIC1+1)
#define PIC2_COMMAND            PIC2
#define PIC2_DATA               (PIC2+1)

/*
The PIC has a complex initialization sequence. We must send a series
of "Initialization Command Words" (ICWs) to its command ports.
*/

void pic_remap(void) {
    // Start the initialization sequence in cascade mode
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    // Set the new Vector offsets for the PICs
    outb(PIC1_DATA, 32);            // Master PIC will use interrupts 32-39
    outb(PIC2_DATA, 40);            // Slave PIC will use interrupts 40-47

    // Tell the master PIC that there is a slave PIC at IRQ2
    outb(PIC1_DATA, 4);
    // Tell the slave PIC its cascade identity(2)
    outb(PIC2_DATA, 2);

    // Set the PICs to 8086/88 (standard) mode
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
}

/*
Sends an End-of-Interrupt (EOI) signal to the PICs.
This must be done at the end of every interrupt handler.
*/
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20);       // Send EOI to slave PIC
    }
    outb(PIC1_COMMAND, 0x20);           // Send EOI to master PIC
}
