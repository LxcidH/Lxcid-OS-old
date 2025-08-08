; =============================================================================
; LxcidOS Kernel - Restructured
; Author: Lxcid
; Description: This is the user's code, reorganized into logical sections
;              for better readability and maintenance. Duplicate labels have
;              been fixed to allow the code to assemble.
; =============================================================================

[bits 16]
[org 0x8000]

; =============================================================================
; Color Constants
; Pass into BL register before calling print_string_color to color the text
; =============================================================================

COLOR_BLACK         equ     00h
COLOR_BLUE          equ     01h
COLOR_GREEN         equ     02h
COLOR_CYAN          equ     03h
COLOR_RED           equ     04h
COLOR_MAGENTA       equ     05h
COLOR_BROWN         equ     06h
COLOR_LIGHT_GRAY    equ     07h
COLOR_DARK_GRAY     equ     08h
COLOR_LIGHT_BLUE    equ     09h
COLOR_LIGHT_GREEN   equ     0Ah
COLOR_LIGHT_CYAN    equ     0Bh
COLOR_LIGHT_RED     equ     0Ch
COLOR_LIGHT_MAGENTA equ     0Dh
COLOR_YELLOW        equ     0Eh
COLOR_WHITE         equ     0Fh

; =============================================================================
; KERNEL ENTRY POINT
; =============================================================================
start:
    call clear_screen
    mov si, welcome_msg
    mov bl, COLOR_MAGENTA
    call print_string_color

; =============================================================================
; MAIN SHELL LOOP
; =============================================================================
shell_loop:
    call print_nl
skip_nl:
    mov si, prompt
    mov bl, COLOR_MAGENTA
    call print_string_color

    ; Read command from the user into our buffer
    mov di, cmd_buffer
    call read_command

    ; --- Parse the command ---
    mov si, cmd_buffer
    mov di, help_cmd_str
    call strcmp
    jc .run_help

    mov si, cmd_buffer
    mov di, reboot_cmd_str
    call strcmp
    jc .run_reboot

    mov si, cmd_buffer
    mov di, reboot_cmd_str
    call strcmp
    jc .run_shutdown

    mov si, cmd_buffer
    mov di, clear_cmd_str
    call strcmp
    jc .run_clear

    ; --- Special check for 'echo' command ---
    ; We load the first 4 bytes of the user's input and see if it
    ; matches the 4 bytes of the string 'echo'
    mov eax, [cmd_buffer]
    cmp eax, 'echo'     ; 'ohce' is 'echo' in little-endian
    je .run_echo

    ; --- Command not found ---
    mov si, not_found_msg
    mov bl, COLOR_RED
    call print_nl
    call print_string_color
    jmp shell_loop

.run_help:
    call cmd_help
    jmp shell_loop

.run_reboot:
    call cmd_reboot
    ; This command does not return
    ; but if it fails we will loop
    jmp shell_loop

.run_shutdown:
    call cmd_shutdown
    ; This command doesn't return

.run_clear:
    call clear_screen
    jmp skip_nl ; FIX: Added jump to prevent fall-through bug

.run_echo:
    call cmd_echo
    jmp shell_loop

; =============================================================================
; KERNEL FUNCTIONS (I/O and String Utilities)
; =============================================================================

; --- Function: print_string_color ---
; Prints a null-terminated string with a specified color, and correctly.
; advances the cursor after each character
; Input: SI = address of the string, BL = color attribute.
print_string_color:
    pusha                   ; Push all registers to the stack
.loop_psc:
    lodsb
    cmp al, 0
    je .done_psc

    ; Get current cursor position
    mov ah, 03h
    mov bh, 00h
    int 0x10

    ; Print character at current cursor pos
    mov ah, 09h
    mov bh, 00h
    mov cx, 1               ; Print one character
    int 0x10

    inc dl
    cmp dl,80               ; Are we at the end of the line?
    jne .set_cursor
    mov dl, 0               ; wrap to next line
    inc dh

.set_cursor:
    mov ah, 02h              ; Set cursor pos func
    mov bh, 00h
    int 0x10

    jmp .loop_psc

.done_psc:
    popa
    ret

; --- Function: print_nl ---
; Prints a newline (carriage return + line feed).
print_nl:
    mov ah, 0eh
    mov al, 0dh
    int 10h
    mov al, 0ah
    int 10h
    ret

; --- Function: read_command ---
; Reads keystrokes from the user into a buffer until Enter is pressed.
; Input: DI = address of the buffer.
read_command:
.loop_rc:
    mov ah, 00h
    int 16h

    cmp al, 0dh      ; Check for enter key
    je .done_rc
    cmp al, 08h      ; Check for backspace
    je .backspace_rc

    stosb            ; Store char in AL at [DI], and increment DI
    mov ah, 0eh      ; Echo char back to screen
    int 10h
    jmp .loop_rc

.backspace_rc:
    cmp di, cmd_buffer ; Don't backspace if the buffer is empty
    je .loop_rc
    dec di             ; Move buffer pointer back one
    mov byte [di],0    ; Erase character from buffer
    mov ah, 0eh        ; Erase char from screen
    mov al, 08h
    int 10h
    mov al, ' '
    int 10h
    mov al, 08h
    int 10h
    jmp .loop_rc

.done_rc:
    mov byte [di], 0   ; Null-terminate the string in the buffer
    ret

; --- Function: strcmp ---
; Compares two null-terminated strings.
; Input: SI = string 1, DI = string 2
; Output: Sets Carry Flag (CF=1) if equal.
strcmp:
.loop_sc:
    mov al, [si]
    mov cl, [di]
    cmp al, cl
    jne .not_equal_sc
    cmp al, 0
    je .equal_sc
    inc si
    inc di
    jmp .loop_sc
.equal_sc:
    stc
    ret
.not_equal_sc:
    clc
    ret

; =============================================================================
; COMMAND HANDLERS
; =============================================================================

cmd_help:
    call print_nl
    mov si, help_msg_1
    mov bl, COLOR_MAGENTA
    call print_string_color
    call print_nl
    mov si, help_msg_2
    mov bl, COLOR_WHITE
    call print_string_color
    ret

cmd_echo:
    mov si, cmd_buffer

.find_space_loop:
    lodsb
    cmp al, ' '
    je .found_space
    cmp al, 0
    je .no_arg
    jmp .find_space_loop

.found_space:
    ; SI now points to start of args
    call print_nl
    mov bl, COLOR_WHITE
    call print_string_color
    ret

.no_arg:
    call print_nl
    mov si, no_args_msg
    mov bl, COLOR_RED
    call print_string_color
    ret

cmd_reboot:
    jmp 0xffff:0

cmd_shutdown

; =============================================================================
; SYSTEM FUNCTIONS
; =============================================================================

clear_screen:
    mov ah, 00h
    mov al, 03h
    int 10h
    ret

; =============================================================================
; DATA SECTION (.data)
; =============================================================================

welcome_msg: db 'Welcome to the LxcidOS kernel!', 0
prompt:  db 'LxcidOS > ', 0
help_msg_1: db 'LxcidOS | Version: 0.0.2', 0
help_msg_2: db 'Available commands: help, reboot, clear, echo', 0
not_found_msg: db 'Command not found', 0
no_args_msg: db 'You did not provide a argument to the command. Please try again!', 0

help_cmd_str: db 'help', 0
reboot_cmd_str: db 'reboot', 0
clear_cmd_str: db 'clear', 0
echo_cmd_str: db 'echo', 0
shutdown_cmd_str: db 'shutdown', 0
; =============================================================================
; UNINITIALIZED DATA SECTION (.bss)
; =============================================================================
section .bss
cmd_buffer: resb 64
