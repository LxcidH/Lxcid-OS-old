; This is the Multiboot header.
section .multiboot
    dd 0x1BADB002             ; Magic number
    dd 0x03                   ; Flags: 0x01 (page_align) | 0x02 (mem_info)
    dd -(0x1BADB002 + 0x03)   ; Checksum

; Reserve space for the stack in the .bss section (uninitialized data)
section .bss
resb 16384 ; Reserve 16KB for the stack
stack_top:

section .text
extern kmain
global start

start:
    ; --- THIS IS THE FIX ---
    ; Set the stack pointer to the top of the stack we just reserved
    mov esp, stack_top

    ; Now we can safely push and call
    push ebx    ; Push the Multiboot info pointer
    call kmain  ; Call our C kernel

    ; Halt the CPU if kmain ever returns
    cli
    hlt
