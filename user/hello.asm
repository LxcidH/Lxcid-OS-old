bits 32
org 0x150000

section .text
    global _start

_start:
    ; --- sys_write ---
    mov eax, 4          ; Syscall number for write
    mov ebx, 1          ; File descriptor 1 (stdout)
    mov ecx, msg        ; Pointer to the message
    mov edx, msg_len    ; Length of the message
    int 0x80            ; Call the kernel

    ; --- sys_exit ---
    mov eax, 1          ; Syscall number for exit
    int 0x80            ; Call the kernel

section .data
    msg: db 'Hello from an ASM program!', 0x0A ; Message with a newline
    msg_len: equ $ - msg
