#include "idt.h"

// Declare the asm stub for our default interrupt handler
extern void isr_stub_common(void);

// Define the IDT array and the IDT Descriptor
struct idt_gate_t idt[256];
struct idt_descriptor_t idt_ptr;

// Helper function to set an entry in the IDT
void idt_set_gate(int n, uint32_t offset, uint16_t selector, uint8_t flags) {
    idt[n].offset_low = (offset & 0xFFFF);
    idt[n].offset_high = (offset >> 16) & 0xFFFF;
    idt[n].selector = selector;
    idt[n].flags = flags;
    idt[n].zero = 0;
}

// Initializes the IDT
void idt_init(void) {
    // Set up the IDT pointer
    idt_ptr.limit = (sizeof(struct idt_gate_t) * 256) -1;
    idt_ptr.base = (uint32_t)&idt;

    // For now point all interrupts to a default handler
    // use 0 as place holder for address (TEMP)
    for(int i = 0; i < 256; i++) {
        // The flags 0x8E mean: Present, Ring 0, 32-bit interrupt gate
        idt_set_gate(i, 0, 0x08, 0x8E);
    }
}
