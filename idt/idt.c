#include "idt.h"
#include "../drivers/terminal.h" // Relative paths may vary
#include "../drivers/pic.h"
#include "../drivers/keyboard.h"
#include "../src/syscall.h"

// --- Extern declarations for assembly ISR stubs ---
// These are the low-level entry points defined in interrupts.asm
extern void isr0(); extern void isr1(); extern void isr2(); extern void isr3();
extern void isr4(); extern void isr5(); extern void isr6(); extern void isr7();
extern void isr8(); extern void isr9(); extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();
extern void isr128(void);

extern void irq0(); extern void irq1(); extern void irq2(); extern void irq3();
extern void irq4(); extern void irq5(); extern void irq6(); extern void irq7();
extern void irq8(); extern void irq9(); extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();

// This is our safe, assembly-defined default handler
extern void default_handler();

// We need to declare the IDT and its pointer here so they can be accessed.
struct idt_entry_struct idt_entries[256];
struct idt_ptr_struct   idt_ptr;

// The C wrapper for the 'lidt' instruction.
static inline void idt_load(struct idt_ptr_struct* idt_ptr) {
    asm volatile ("lidt (%0)" : : "r"(idt_ptr));
}

// --- C-level Interrupt Handlers ---

// This function handles all CPU exceptions (ISRs 0-31)
void fault_handler(registers_t* regs) {
    terminal_writeerror("EXCEPTION: %d - System Halted.", regs->int_no);
    for (;;);
}

// This function handles all hardware interrupts (IRQs 0-15)
void irq_handler(registers_t* regs) {
    // Dispatch to the correct driver based on the hardware interrupt number
    switch (regs->int_no) {
        case 33: // IRQ 1: Keyboard
            keyboard_handler();
            break;

        // Add more cases here for other hardware like mice, disks, etc.
        default:
            // You can optionally print a message for unhandled IRQs
            break;
    }
}

// --- Unified Interrupt Dispatcher ---
// This is the single C function called by the assembly stub. It decides whether
// the interrupt is a fault, an IRQ, or a syscall.
void c_interrupt_handler(registers_t* regs) {
    if (regs->int_no == 128) {
        // It's a system call, so pass the registers to the syscall handler
        syscall_handler(regs);
    } else if (regs->int_no >= 32 && regs->int_no <= 47) {
        // It's a hardware interrupt (IRQ).
        irq_handler(regs);
        // We MUST send an End-of-Interrupt (EOI) to the PICs for IRQs.
        pic_send_eoi(regs->int_no - 32);
    } else {
        // It's a CPU exception.
        fault_handler(regs);
    }
}

// --- IDT Setup ---

// Helper function to set an entry in the IDT
static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_low = (base & 0xFFFF);
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].selector = sel;
    idt_entries[num].flags = flags;
    idt_entries[num].always0 = 0;
}

// Initializes the Interrupt Descriptor Table
void idt_init(void) {
    idt_ptr.limit = (sizeof(struct idt_entry_struct) * 256) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    // --- Defensive Initialization ---
    // First, set all 256 entries to the safe default handler.
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)default_handler, 0x08, 0x8E);
    }

    // --- ISRs (CPU Exceptions) ---
    void* isr_routines[] = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7, isr8, isr9, isr10,
        isr11, isr12, isr13, isr14, isr15, isr16, isr17, isr18, isr19, isr20,
        isr21, isr22, isr23, isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };
    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, (uint32_t)isr_routines[i], 0x08, 0x8E);
    }

    // --- IRQs (Hardware Interrupts) ---
    void* irq_routines[] = {
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7, irq8,
        irq9, irq10, irq11, irq12, irq13, irq14, irq15
    };
    for (int i = 0; i < 16; i++) {
        idt_set_gate(32 + i, (uint32_t)irq_routines[i], 0x08, 0x8E);
    }

    // --- System Call Gate ---
    // CRITICAL FIX: Set the flags to 0xEE instead of 0x8E.
    // 0xEE = Present(1), DPL=3(11), Type=Trap Gate(1110)
    // This allows user-mode (Ring 3) programs to call 'int 0x80'.
    idt_set_gate(128, (uint32_t)isr128, 0x08, 0xEE);

    // Load the IDT using the assembly instruction.
    idt_load(&idt_ptr);
}
