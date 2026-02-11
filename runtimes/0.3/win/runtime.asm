format MS64 COFF

public x64_fastPushObject
public x64_fastQueryObject
public x64_fastNewObject
public x64_fastCallObject
public x64_fastPushPipe
public x64_fastQueryPipe
public x64_fastSleep
public gpu_fastNewObject
public gpu_fastCallObject
public loc_fastNewObject
public dll_fastCallObject
public any_fastCastProvider

public x64AsmExecuteWorker

public DllCall

section '.data' readable writable align 64
; ! important that invoke data is strictly before runtime data, becouse it will be used as (rbp-XX)
context db 512 dup 0

extrn x64QueryObject
extrn x64QueryPipe
extrn x64PushObject
extrn x64PushPipe
extrn x64NewObject
extrn x64Sleep
extrn x64CallObject
extrn gpuNewObject
extrn gpuCallObject
extrn locNewObject
extrn dllCallObject
extrn anyCastProvider
extrn myPrintf

fmt du 'bad value: %lld', 10, 0

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
    
    sub rsp, 32
}

macro LeaveCCode {
    ; load used registers
    add rsp, 32

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


macro CWrapper endpoint {
    mov rax, rbp
    sub rax, 1024
    StoreContext rax
    mov rax, [rsp]
    EnterCCode
    mov r8, rax
    mov r9, rbp
    ;test rsp, 0xF
    ;jnz .BAD
    call endpoint
    mov rdi, rax
    LeaveCCode
    ret
;.BAD:
    ;sub rsp, 32

    ;mov rcx, fmt
    ;mov rdx, rsp

    ;call myPrintf
    
    ;add rsp, 32
    
    ;LeaveCCode
    ;ret
}


; arguments:
; fn(rdi, rsi, rdx, rcx, returnAddress, rbpValue)
x64_fastPushObject:
    CWrapper x64PushObject
x64_fastQueryObject:
    CWrapper x64QueryObject
x64_fastNewObject:
    CWrapper x64NewObject
x64_fastCallObject:
    CWrapper x64CallObject
x64_fastPushPipe:
    CWrapper x64PushPipe
x64_fastQueryPipe:
    CWrapper x64QueryPipe
x64_fastSleep:
    CWrapper x64Sleep
gpu_fastNewObject:
    CWrapper gpuNewObject
gpu_fastCallObject:
    CWrapper gpuCallObject
loc_fastNewObject:
    CWrapper locNewObject
dll_fastCallObject:
    CWrapper dllCallObject
any_fastCastProvider:
    CWrapper anyCastProvider

; c style function ExecuteWorker(void *address, int64_t rdi_value, void *rbp_value, void *context)
x64AsmExecuteWorker:
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
;    int64_t call_stack_usage;   // +24 // precalculated stack usage value
DllCall:
    push r12
    push r13
    push r14
    push r15
    push rdi

    mov r12, QWORD [rcx + 24]
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
    
