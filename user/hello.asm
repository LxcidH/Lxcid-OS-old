; Assembly code to create a flat binary that prints a message
; and then exits using LxcidOS system calls.

; Standard x86 architecture register usage for syscalls:
; EAX: System call number
; EBX: First argument
; ECX: Second argument
; EDX: Third argument

BITS 32
ORG 0x150000
; --- Start of the code section ---
start:
    ; --- Syscall 4: WRITE ---
    ; int write(int fd, const char* buf, size_t count);

    mov     eax, 4           ; System call number for WRITE is 4
    mov     ebx, 1           ; First argument: file descriptor 1 (stdout)
    mov     ecx, msg         ; Second argument: pointer to the message
    mov     edx, len         ; Third argument: length of the message

    int     0x80             ; Trigger the system call interrupt

    ; --- Syscall 1: EXIT ---
    ; void exit(int status);

    mov     eax, 1           ; System call number for EXIT is 1
    mov     ebx, 0           ; First argument: exit status 0 (success)

    int     0x80             ; Trigger the system call interrupt

; --- Data section ---
msg     db  "Hello from LxcidOS!", 0x0A  ; The message string, 0x0A is a newline character
len     equ $ - msg                   ; 'len' is the length of the string
