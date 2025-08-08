; This is the Multiboot header.
section .multiboot
    dd 0x1BADB002            ; Magic number
    dd 0x03                  ; Flags: We want memory info and video mode
    dd -(0x1BADB002 + 0x03)   ; Checksum

section .text
extern kmain
extern idt_ptr
global start
global idt_load

idt_load:
    lidt [idt_ptr]
    ret

start:
    ; The bootloader (QEMU) has already put us in 32-bit protected mode.
    ; It passes a pointer to the Multiboot info structure in the EBX register.
    ; We push it onto the stack to pass it as an argument to kmain.
    push ebx
    call kmain

    ; Halt the CPU if kmain ever returns
    cli
    hlt
