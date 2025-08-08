section .text
global outb
global inb

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
