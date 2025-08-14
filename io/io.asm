section .text
global outb
global inb
global insw
global outsw

; outb; sends a byte to an I/O port
; stack: [esp+8]: data, [esp+4]: port
outb:
    mov al, [esp + 8]   ; Get the data byte from the stack
    mov dx, [esp + 4]   ; Get the port number from the stack
    out dx, al          ; Execture the 'out' instruction
    ret

; inb: receives a byte from an I/O port
; stack: [esp+4]: port
inb:
    mov dx, [esp + 4]   ; Get the port number from the stack
    in al, dx           ; Execute the 'in' instruction
    ret                 ; Result is returned in the AL register

; void insw(uint16_t port, void* addr, uint32_t count);
insw:
    push ebp
    mov ebp, esp

    mov edx, [ebp + 8]          ; Port to read from -> into DX
    mov edi, [ebp + 12]         ; Buffer address -> into EDI
    mov ecx, [ebp + 16]         ; Count of words -> into CX

    cld                         ; Clear direction flag, to increment EDI
    rep insw                     ; Repeat CX times: read word from [DX] to [ES:EDI]

    pop ebp
    ret

outsw:
    push ebp
    mov ebp, esp

    mov edx, [ebp + 8]          ; Port to write to -> DX
    mov esi, [ebp + 12]         ; Buffer address -> ESI (source)
    mov ecx, [ebp + 16]         ; Count of words -> CX

    cld
    rep outsw

    pop ebp
    ret
