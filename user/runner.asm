extern main

; This is the syscall number for 'exit' (you'll need to define this)
SYS_EXIT equ 1 

global _start     ; The linker looks for this label

section .text
_start:
    ; The C code needs a stack to work with. The OS loader
    ; should provide a stack pointer in 'esp' before jumping here.

    call main     ; Call the main function from your C file.

    ; After main returns, we exit the program.
    mov eax, SYS_EXIT ; Load the exit syscall number
    int 0x80          ; Make the syscall
