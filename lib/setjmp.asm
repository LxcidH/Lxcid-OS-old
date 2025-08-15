; In lib/setjmp.asm
global setjmp
global longjmp

section .text

setjmp: ; int setjmp(jmp_buf env)
    mov eax, [esp + 4]
    mov [eax], ebx
    mov [eax+4], esp
    mov [eax+8], ebp
    mov [eax+12], esi
    mov [eax+16], edi
    mov ecx, [esp]
    mov [eax+20], ecx
    xor eax, eax
    ret

longjmp: ; void longjmp(jmp_buf env, int val)
    mov ebx, [esp + 4]  ; Get jmp_buf pointer
    mov eax, [esp + 8]  ; Get return value

    ; If return value is 0, return 1 instead
    test eax, eax
    jnz .do_jump
    inc eax

.do_jump:
    ; Restore state from jmp_buf
    mov esp, [ebx+4]
    mov ebp, [ebx+8]
    mov esi, [ebx+12]
    mov edi, [ebx+16]
    mov ecx, [ebx+20]   ; Get target EIP

    mov ebx, [ebx]      ; Restore EBX last

    jmp ecx             ; Jump to the saved EIP
