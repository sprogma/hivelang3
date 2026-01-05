format MS64 COFF

; debugPrint = 1

public fastPushObject
public fastNewObject
public fastCallObject
public fastQueryObject

public callExample


section '.data' readable writable

invoke_data db 512 dup 0
runtime_data db 512 dup 0

format_strA db 'PushObject %p+%lld ', 0
format_strB db '<- %p of size %lld', 0xA, 0
format_strC db 'QueryObject %p <- %p', 0
format_strD db '+%lld of size %lld', 0xA, 0
    
extrn PushObject
extrn NewObject
extrn QueryObject
extrn CallObject

extrn object_array
extrn printf

section '.text' code readable executable

macro EnterCCode {
    ; save used registers
    push r8
    push r9
    push r10
    push r11
    push r12
    sub rsp, 0+32
}

macro LeaveCCode {
    ; load used registers
    add rsp, 0+32
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
}

; rcx=size
; rdx=offset
; rdi=object
; rsi=source/value
fastPushObject:
    ; TODO: add support of remote objects

    if defined debugPrint
    
        EnterCCode

        mov r12, rcx
        mov r13, rdx
        mov r14, rdi
        mov r15, rsi
        
        lea rcx, [format_strA]
        mov rdx, r14
        mov r8, r13
        call printf
        
        lea rcx, [format_strB]
        mov rdx, r15
        mov r8, r12
        call printf

        mov rcx, r12
        mov rdx, r13
        mov rdi, r14
        mov rsi, r15
        
        LeaveCCode

    end if

    mov BYTE [rdi + 1], 1 ; mark object as set
    add rdi, 2 ; common object header size
    
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

; reverse to fastPushObject
; rsi=object
; rdi=source/value
; rcx=size
; rdx=offset
fastQueryObject:
    ; TODO: add support of remote objects
    
    if defined debugPrint

        EnterCCode

        mov r12, rcx
        mov r13, rdx
        mov r14, rdi
        mov r15, rsi
        
        lea rcx, [format_strC]
        mov rdx, r14
        mov r8, r15
        call printf
        
        lea rcx, [format_strD]
        mov rdx, r13
        mov r8, r12
        call printf

        mov rcx, r12
        mov rdx, r13
        mov rdi, r14
        mov rsi, r15
        
        LeaveCCode

    end if

    add rsi, 2 ; common object header size
    
    ; for now, simply move to object
    cmp rcx, 0
    jg .copy_blockq
    ; size < 0 -> need to move rsi to dest
    cmp rcx, -8
    jne .not64q
    mov rdi, [rsi + rdx]
    ret
.not64q:
    cmp rcx, -4
    jne .not32q
    mov edi, [rsi + rdx]
    ret
.not32q:
    cmp rcx, -2
    jne .not16q
    movzx edi, WORD [rsi + rdx]
    ret
.not16q:
    movzx edi, BYTE [rsi + rdx]
    ret
.copy_blockq:
    add rsi, rdx
    rep movsb
    ret




; for now, only enter c code and call new object - no fast version
; so
; rcx=type
; rdx=size
; rdi=param
; result is in rax
fastNewObject:
    EnterCCode

    mov r8, rdi
    
    call NewObject
    
    LeaveCCode

    ret

; 
fastCallObject:
    mov eax, 42         ; Example: return 42
    ret


callExample:
    sub rsp, 8
    push rbp

    push rcx
    sub rsp, 32 ; shadow space?

    mov rcx, 2
    mov rdx, 8
    mov r8, 8
    call NewObject

    ; prepare args
    lea rbp, [runtime_data]
    lea rdi, [invoke_data]
    
    mov QWORD [rdi + 4], rax
    
    ; mov DWORD [rdi + 4], 13
    mov DWORD [rdi + 0], 17

    pop rax
    call rax
    
    add rsp, 32
    pop rbp
    add rsp, 8
    ret
