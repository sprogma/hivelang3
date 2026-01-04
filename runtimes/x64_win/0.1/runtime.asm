format MS64 COFF

public fastPushObject
public fastNewObject
public fastCallObject
public fastQueryObject

public callExample


section '.data' readable writable

invoke_data db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
runtime_data db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

format_strA db 'PushObject %p+%lld ', 0
format_strB db '<- %p of size %lld', 0xA, 0
    
extrn PushObject
extrn NewObject
extrn QueryObject
extrn CallObject

extrn object_array
extrn printf

section '.text' code readable executable

macro EnterCCode {
    ; save used registers
    sub rsp, 8
    push r8
    push r9
    push r10
    push r11
}

macro LeaveCCode {
    ; load used registers
    pop r11
    pop r10
    pop r9
    pop r8
    add rsp, 8
}



; rcx=size
; rdx=offset
; rdi=object
; rsi=source/value
fastPushObject:
    ; TODO: add support of remote objects

    EnterCCode

    mov r12, rcx
    mov r13, rdx
    mov r14, rdi
    mov r15, rsi

    sub rsp, 32
    
    lea rcx, [format_strA]
    mov rdx, r14
    mov r8, r13
    call printf
    
    lea rcx, [format_strB]
    mov rdx, r15
    mov r8, r12
    call printf
    
    add rsp, 32

    mov rcx, r12
    mov rdx, r13
    mov rdi, r14
    mov rsi, r15
    
    LeaveCCode
    
    ; for now, simply move to object
    cmp rcx, 0
    jg .copy_block
    ; size < 0 -> need to move rsi to dest
    cmp rcx, -8
    jne .not64
    mov [rdi + rdx], rsi
    ret
.not64:
    cmp rcx, -4
    jne .not32
    mov [rdi + rdx], esi
    ret
.not32:
    cmp rcx, -2
    jne .not16
    mov [rdi + rdx], si
    ret
.not16:
    mov BYTE [rdi + rdx], sil
    ret
.copy_block:
    add rdi, rdx
    rep movsb
    ret
    

; 
fastNewObject:
fastQueryObject:
fastCallObject:
    mov eax, 42         ; Example: return 42
    ret


callExample:
    sub rsp, 8
    push rbp

    push rcx

    mov rcx, 2
    mov rdx, 8
    call NewObject

    ; prepare args
    lea rbp, [runtime_data]
    lea rdi, [invoke_data]
    
    mov QWORD [rdi + 8], rax
    
    mov DWORD [rdi + 4], 13
    mov DWORD [rdi + 0], 17

    pop rax
    call rax
    
    pop rbp
    add rsp, 8
    ret
