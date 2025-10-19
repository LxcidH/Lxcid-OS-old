#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// A single entry in the Interrupt Descriptor Table (IDT).
struct idt_entry_struct {
    uint16_t base_low;      // The lower 16 bits of the address to jump to.
    uint16_t selector;      // The kernel segment selector.
    uint8_t  always0;       // This must always be zero.
    uint8_t  flags;         // The entry's type and attributes.
    uint16_t base_high;     // The upper 16 bits of the address to jump to.
} __attribute__((packed));

// The structure the 'lidt' instruction needs to locate the IDT.
struct idt_ptr_struct {
    uint16_t limit;         // The size of the IDT in bytes - 1.
    uint32_t base;          // The base address of the IDT.
} __attribute__((packed));

// This struct defines the data pushed onto the stack by our assembly stubs.
// It must precisely match the order of registers pushed.
typedef struct registers {
    uint32_t ds;                                      // Pushed by our stub
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;   // Pushed by 'pusha'
    uint32_t int_no, err_code;                        // Pushed by the ISR macro
    uint32_t eip, cs, eflags, useresp, ss;            // Pushed by the processor automatically
} registers_t;

// The main initialization function for the IDT.
void idt_init(void);

#endif
