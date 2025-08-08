#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* The structure for an entry in the IDT */
struct idt_gate_t {
    uint16_t offset_low;    // Lower 16 bits of the handler function address
    uint16_t selector;      // Kernel code segment selector
    uint8_t  zero;          // This must always be zero
    uint8_t  flags;         // Flags. See documentation.
    uint16_t offset_high;   // Upper 16 bits of the handler function address
} __attribute__((packed));

/* The structure for the IDT pointer */
struct idt_descriptor_t {
    uint16_t limit;         // The size of the IDT
    uint32_t base;          // The address of the first IDT entry
} __attribute__((packed));


// Declare our IDT and its descriptor
extern struct idt_gate_t idt[256];
extern struct idt_descriptor_t idt_ptr;

/* A function to load our IDT */
void idt_load();

#endif
