section .text
global isr_stub_common

; This is the default interrupt handler that all 256 IDT entries will
; point to. For now, it does nothing but return
isr_stub_common:
    ; In future we need to push registers, call a C handler,
    ; send an EOI (End of Interrupt) signal, pop registers and then return
    iret    ; Use iret to return from an interrupt
