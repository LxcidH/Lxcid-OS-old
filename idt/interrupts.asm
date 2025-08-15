; =========================================================================
;              interrupts.asm - Low-Level Interrupt Stubs
;                       (Full, Corrected Version)
; =========================================================================

; This is the single C function that will handle all interrupts.
extern c_interrupt_handler
extern syscall_handler
; A macro to create an ISR stub for exceptions with no error code.
%macro ISR_NO_ERR_CODE 1
    global isr%1
    isr%1:
        push dword 0    ; Push a dummy error code (0).
        push dword %1   ; Push the interrupt number.
        jmp common_handler_stub
%endmacro

; A macro for exceptions that DO push an error code.
%macro ISR_ERR_CODE 1
    global isr%1
    isr%1:
        ; The CPU pushes the error code automatically.
        push dword %1   ; Push the interrupt number.
        jmp common_handler_stub
%endmacro

; A macro for hardware interrupts (IRQs).
%macro IRQ 2
    global irq%1
    irq%1:
        push dword 0    ; Push a dummy error code (0).
        push dword %2   ; Push the interrupt number (32 + IRQ number).
        jmp common_handler_stub
%endmacro

; --- The Unified Common Handler Stub -------------------------------------
; This is the core logic that all ISRs and IRQs jump to.
common_handler_stub:
    pusha           ; Save all general-purpose registers.

    mov ax, ds      ; Save the data segment selector.
    push eax

    mov ax, 0x10    ; Load the kernel data segment selector.
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp    ; Get a pointer to the registers struct on the stack
    push eax        ; Push pointer as an argument for the C handler

    call c_interrupt_handler ; Call our unified C handler.

    add esp, 4      ; Clean up the pointer argument from the stack.

    pop eax         ; Restore the original data segment.
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa            ; Restore general-purpose registers.
    add esp, 8      ; Clean up the pushed error code and interrupt number.
    ; NO 'sti' here!
    iret            ; Atomically restore state and return from interrupt.

; =========================================================================
;                      MUST INCLUDE ALL DEFINITIONS!
; =========================================================================

; --- Generate all 32 ISR stubs (CPU Exceptions 0-31) ---
ISR_NO_ERR_CODE 0
ISR_NO_ERR_CODE 1
ISR_NO_ERR_CODE 2
ISR_NO_ERR_CODE 3
ISR_NO_ERR_CODE 4
ISR_NO_ERR_CODE 5
ISR_NO_ERR_CODE 6
ISR_NO_ERR_CODE 7
ISR_ERR_CODE    8
ISR_NO_ERR_CODE 9
ISR_ERR_CODE    10
ISR_ERR_CODE    11
ISR_ERR_CODE    12
ISR_ERR_CODE    13
ISR_ERR_CODE    14
ISR_NO_ERR_CODE 15
ISR_NO_ERR_CODE 16
ISR_ERR_CODE    17
ISR_NO_ERR_CODE 18
ISR_NO_ERR_CODE 19
ISR_NO_ERR_CODE 20
ISR_NO_ERR_CODE 21
ISR_NO_ERR_CODE 22
ISR_NO_ERR_CODE 23
ISR_NO_ERR_CODE 24
ISR_NO_ERR_CODE 25
ISR_NO_ERR_CODE 26
ISR_NO_ERR_CODE 27
ISR_NO_ERR_CODE 28
ISR_NO_ERR_CODE 29
ISR_ERR_CODE    30
ISR_NO_ERR_CODE 31
ISR_NO_ERR_CODE 128 ; This is 0x80, for system calls

; --- Generate all 16 IRQ stubs (Hardware Interrupts 32-47) ---
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; A safe default handler that just returns, in case we jump to an
; unassigned interrupt vector.
global default_handler
default_handler:
    iret
