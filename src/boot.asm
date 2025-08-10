; This is the Multiboot header.
section .multiboot
    dd 0x1BADB002            ; Magic number
    dd 0x03                  ; Flags: 0x01 (page_align) | 0x02 (mem_info)
    dd -(0x1BADB002 + 0x03)   ; Checksum

section .text
extern kmain
global start

start:
    ; The bootloader (QEMU) has already put us in 32-bit protected mode.
    ; It passes a pointer to the Multiboot info structure in the EBX register.
    ; We push it onto the stack to pass it as an argument to kmain.
    push ebx
    call kmain

    ; Halt the CPU if kmain ever returns
    cli
    hlt
