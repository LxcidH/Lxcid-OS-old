[org 0x7C00]
[bits 16]

start:
    mov si, msg
    call jmp_kernel
    hlt

move_cursor:
    push ax         ; push ax since AX gets zero'd after we get the cursor pos
    mov ah, 03h     ; Get cursor pos && shape
    mov bh, 0       ; Page number
    int 10h

    pop ax          ; Get AX back since AL contains the char we wish to print
    mov ah, 02h     ; Set cursor Pos
                    ; dh stores the current column
    inc dl          ; Increment dl (dl == cursor row)
    int 10h
    call print_string

print_string:
    mov ah, 09h             ; Write character & attribute
    mov bh, 0               ; Page number
    mov bl, 5               ; Magenta (lower 4 bits for foreground, higher 4 bits for background color)
    mov cx, 1               ; Num of times to print char

.loop:
    lodsb
    cmp al, 0               ; Check for null terminator
    je .done
    int 10h
    call move_cursor
    jmp .loop

.done:
    ret

jmp_kernel:
    mov ah, 02h             ; AH = 02h, is the "Read Sectors" Function
    mov al, 1               ; Read 1 sector
    mov ch, 0               ; Cylinder 0
    mov cl, 2               ; Start at sector 2 (sector 1 is our bootloader)
    mov dh, 0               ; Head 0
    mov bx, 0x8000          ; Load the kernel to address 0x8000
    int 0x13                ; Call the disk read interrupt

    jc disk_error           ; If carry flag is set, there was an error

    jmp 0x8000              ; Jump to the loaded kernel code

disk_error:
    mov si, error_msg
    call print_string
    hlt

error_msg: db 'Disk Read Error :(', 0
msg: db 'Hello, Bootloader world :3', 0

times 510 - ($-$$) db 0
dw 0xaa55
