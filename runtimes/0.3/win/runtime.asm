format MS64 COFF


public fastPushObject
public fastNewObject
public fastCallObject
public fastQueryObject
public fastPushPipe
public fastQueryPipe

public ExecuteWorker
public DllCall
public context


section '.data' readable writable align 64

; ! important that invoke data is strictly before runtime data, becouse it will be used as (rbp-XX)
invoke_data db 512 dup 0
runtime_data db 512 dup 0
context db 512 dup 0



format_strA du 'PushObject %p+%lld ', 0
format_strB du '<- %p of size %lld', 0xA, 0
format_strC du 'QueryObject %p <- %p', 0
format_strD du '+%lld of size %lld', 0xA, 0
    
extrn PushObject
extrn NewObject
extrn QueryObject
extrn CallObject
extrn QueryPipe
extrn PushPipe
extrn myPrintf

section '.text' code readable executable

macro StoreContext dest {
    mov QWORD [dest], r8
    mov QWORD [dest + 8], r9
    mov QWORD [dest + 16], r10
    mov QWORD [dest + 24], r11
    mov QWORD [dest + 32], r12
    mov QWORD [dest + 40], rbx
    mov QWORD [dest + 48], r13
    mov QWORD [dest + 56], r14
    mov QWORD [dest + 64], r15
}

macro LoadContext dest {
    mov r8, QWORD [dest]
    mov r9, QWORD [dest + 8]
    mov r10, QWORD [dest + 16]
    mov r11, QWORD [dest + 24]
    mov r12, QWORD [dest + 32]
    mov rbx, QWORD [dest + 40]
    mov r13, QWORD [dest + 48]
    mov r14, QWORD [dest + 56]
    mov r15, QWORD [dest + 64]
}

macro EnterCCode {
    ; save used registers
    push r8
    push r9
    push r10
    push r11
    push r12
    
    push rbx
    push r13
    push r14
    push r15
    
    sub rsp, 0+32
}

macro LeaveCCode {
    ; load used registers
    add rsp, 0+32

    pop r15
    pop r14
    pop r13
    pop rbx
    
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
        call myPrintf
        
        lea rcx, [format_strB]
        mov rdx, r15
        mov r8, r12
        call myPrintf

        mov rcx, r12
        mov rdx, r13
        mov rdi, r14
        mov rsi, r15
        
        LeaveCCode

    end if

    ; TODO: add fast arrays and their workaround here
    ; extract return address
    
    ; save context
    StoreContext context
    
    mov rax, [rsp]
    EnterCCode

    sub rsp, 32 ; more shadow bytes for 2 registers

    ; run C code

    mov r8, rax
    mov r9, rbp
    call PushObject

    add rsp, 32

    LeaveCCode
    ret


; rcx=size
; rdx=offset
; rdi=object
; rsi=source/value
fastPushPipe:
    ; save context
    StoreContext context
    
    mov rax, [rsp]
    EnterCCode

    sub rsp, 32 ; more shadow bytes for 2 registers - unused

    mov r8, rax
    mov r9, rbp
    call PushPipe

    add rsp, 32

    LeaveCCode
    ret

; reverse to fastPushObject
; rsi=object
; rdi=dest/value
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
        call myPrintf
        
        lea rcx, [format_strD]
        mov rdx, r13
        mov r8, r12
        call myPrintf

        mov rcx, r12
        mov rdx, r13
        mov rdi, r14
        mov rsi, r15
        
        LeaveCCode

    end if

    ; extract return address

    ; save context
    StoreContext context
    
    mov rax, [rsp]
    EnterCCode

    sub rsp, 32 ; more shadow bytes for 2 registers - unused

    ; run C code
    
    mov r8, rax
    mov r9, rbp
    call QueryObject
    mov rdi, rax
    
    add rsp, 32
    
    LeaveCCode
    ret
    
; reverse to fastPushObject
; rsi=object
; rdi=dest/value
; rcx=size
; rdx=offset
fastQueryPipe:
    ; save context
    StoreContext context
    
    mov rax, [rsp]
    EnterCCode

    sub rsp, 32 ; more shadow bytes for 2 registers - unused

    ; run C code
    
    mov r8, rax
    mov r9, rbp
    call QueryPipe
    mov rdi, rax
    
    add rsp, 32
    
    LeaveCCode
    ret



; for now, only enter c code and call new object - no fast version
; so
; rdi=type
; rsi=size
; rdx=param
; result is in rax
fastNewObject:
    mov rcx, [rsp]
    
    StoreContext context
    
    EnterCCode

    mov r8, rbp
    
    call NewObject

    mov rdi, rax
    
    LeaveCCode

    ret

; rdx=workerId rsi=callTable rdi=parameter
fastCallObject:
    EnterCCode

    mov rcx, rsi
    mov r8, rdi

    call CallObject

    LeaveCCode
    
    ret

; c style function ExecuteWorker(void *address, int64_t rdi_value, void *rbp_value, void *context)
ExecuteWorker:
    push r12
    push r13
    push r14
    push r15
    push rdi
    push rsi
    push rbx
    push rbp


    mov rbp, r8
    mov rdi, rdx
    mov rsi, r9
    
    LoadContext rsi

    ; must be 8 bytes UNaligned before call
    call rcx

    
    pop rbp
    pop rbx
    pop rsi
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    ret
    


public setjmpUN
public longjmpUN

; rcx = ptr to jmp_buf
setjmpUN:
; volatile regs
mov [rcx     ], rbx
mov [rcx +  8], rbp
mov [rcx + 16], rdi
mov [rcx + 24], rsi
mov [rcx + 32], r12
mov [rcx + 40], r13
mov [rcx + 48], r14
mov [rcx + 56], r15
; save rsp
lea rax, [rsp + 8]
mov [rcx + 64], rax
; save ret
mov rax, [rsp]
mov [rcx + 72], rax
xor eax, eax
ret

; rcx = ptr to jmp_buf, rdx = value
longjmpUN:
; load regs
mov rbx, [rcx     ]
mov rbp, [rcx +  8]
mov rdi, [rcx + 16]
mov rsi, [rcx + 24]
mov r12, [rcx + 32]
mov r13, [rcx + 40]
mov r14, [rcx + 48]
mov r15, [rcx + 56]
mov rsp, [rcx + 64]
; return
mov rax, rdx
jmp QWORD [rcx + 72]



; c style function DllCall(struct dll_call_data *data, int64_t *input_data, void *promise)
; struct dll_call_data
;    void *loaded_function;      // + 0
;    int64_t output_size;        // + 8
;    int64_t sizes_len;          // +16
;    int64_t *sizes;             // +24
;    int64_t call_stack_usage;   // +32 // precalculated stack usage value
DllCall:
    push r12
    push r13
    push r14
    push r15
    push rdi

    mov r12, QWORD [rcx + 32]
    mov r13, r8
    mov r14, QWORD [rcx + 8]
    mov r15, QWORD [rcx + 16]
    sub rsp, r12 

    mov rdi, rdx
    mov rax, QWORD [rcx]

    ; move all other arguments to stack
    cmp r15, 4
    jle .L1e
.L1:
    sub r15, 1
    mov rcx, [rdi + 8 * r15]
    mov [rsp + 8 * r15], rcx
    cmp r15, 4
    jg .L1
.L1e:
    ; move data as needed
    mov rcx, [rdi + 0]
    mov rdx, [rdi + 8]
    mov r8, [rdi + 16]
    mov r9, [rdi + 24]

    ; call dll entry
    call rax

    ; if result size is 1 2 4 8 - save result
    test r13, r13
    jz .noRet
    mov BYTE [r13 - 2], 1
    cmp r14, 1
    jne .not1
    mov [r13], al
    jmp .noRet
.not1:
    cmp r14, 2
    jne .not2
    mov [r13], ax
    jmp .noRet
.not2:
    cmp r14, 4
    jne .not4
    mov [r13], eax
    jmp .noRet
.not4:
    mov [r13], rax
.noRet:
    add rsp, r12

    ; restore volatile args
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    ret
    
