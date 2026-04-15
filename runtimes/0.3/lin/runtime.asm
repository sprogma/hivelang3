; NASM Linux x64
default rel
[bits 64]

global x64_fastPushObject
global x64_fastQueryObject
global x64_fastNewObject
global x64_fastCallObject
global x64_fastPushPipe
global x64_fastQueryPipe
global x64_fastSleep
global gpu_fastNewObject
global gpu_fastCallObject
global loc_fastNewObject
global dll_fastCallObject
global any_fastCastProvider
global x64AsmExecuteWorker
global DllCall
global setjmpUN
global longjmpUN

extern x64QueryObject
extern x64QueryPipe
extern x64PushObject
extern x64PushPipe
extern x64NewObject
extern x64Sleep
extern x64CallObject
extern gpuNewObject
extern gpuCallObject
extern locNewObject
extern dllCallObject
extern anyCastProvider

section .data align=64
context: times 512 db 0
fmt:     db "bad value: %lld", 10, 0

section .text

%macro StoreContext 1
    mov [ %1 ],      r8
    mov [ %1 + 8 ],  r9
    mov [ %1 + 16 ], r10
    mov [ %1 + 24 ], r11
    mov [ %1 + 32 ], r12
    mov [ %1 + 40 ], rbx
    mov [ %1 + 48 ], r13
    mov [ %1 + 56 ], r14
    mov [ %1 + 64 ], r15
%endmacro

%macro LoadContext 1
    mov r8,  [ %1 ]
    mov r9,  [ %1 + 8 ]
    mov r10, [ %1 + 16 ]
    mov r11, [ %1 + 24 ]
    mov r12, [ %1 + 32 ]
    mov rbx, [ %1 + 40 ]
    mov r13, [ %1 + 48 ]
    mov r14, [ %1 + 56 ]
    mov r15, [ %1 + 64 ]
%endmacro

%macro LoadExtraContext 1
    LoadContext %1
    mov rax, [ %1 + 80 ]
    mov rcx, [ %1 + 88 ]
    mov rdx, [ %1 + 96 ]
    mov rsi, [ %1 + 72 ]
%endmacro

%macro EnterCCode 0
    push r8
    push r9
    push r10
    push r11
    push r12
    
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro LeaveCCode 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
%endmacro

%macro CWrapper 1
    mov rax, rbp
    sub rax, 1024
    StoreContext rax

    mov rax, [rsp]
    
    EnterCCode
    
    mov r8, rax
    mov r9, rbp
    
    call %1
    
    mov rdi, rax
    
    LeaveCCode
    ret
%endmacro

x64_fastPushObject:  CWrapper x64PushObject
x64_fastQueryObject: CWrapper x64QueryObject
x64_fastNewObject:   CWrapper x64NewObject
x64_fastCallObject:  CWrapper x64CallObject
x64_fastPushPipe:    CWrapper x64PushPipe
x64_fastQueryPipe:   CWrapper x64QueryPipe
x64_fastSleep:       CWrapper x64Sleep
gpu_fastNewObject:   CWrapper gpuNewObject
gpu_fastCallObject:  CWrapper gpuCallObject
loc_fastNewObject:   CWrapper locNewObject
dll_fastCallObject:  CWrapper dllCallObject
any_fastCastProvider: CWrapper anyCastProvider

; ExecuteWorker(address, rdi_val, rbp_val, context)
; rdi=addr, rsi=rdi_val, rdx=rbp_val, rcx=context
x64AsmExecuteWorker:
    push r12
    push r13
    push r14
    push r15
    push rbx
    push rbp

    push 0
    push rdi

    mov r12, rdi
    mov rbp, rdx
    mov rdi, rsi
    mov rsi, rcx
    
    LoadExtraContext rsi
    
    call [rsp]

    add rsp, 16

    pop rbp
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    ret

setjmpUN:
    mov [rdi],      rbx
    mov [rdi + 8],  rbp
    mov [rdi + 16], r12
    mov [rdi + 24], r13
    mov [rdi + 32], r14
    mov [rdi + 40], r15
    lea rax, [rsp + 8]
    mov [rdi + 48], rax
    mov rax, [rsp]
    mov [rdi + 56], rax
    xor eax, eax
    ret

longjmpUN:
    mov rbx, [rdi]
    mov rbp, [rdi + 8]
    mov r12, [rdi + 16]
    mov r13, [rdi + 24]
    mov r14, [rdi + 32]
    mov r15, [rdi + 40]
    mov rsp, [rdi + 48]
    mov rax, rsi
    test eax, eax
    jnz .not_zero
    inc eax
.not_zero:
    jmp [rdi + 56]

; DllCall(data, input_data, output_promise)
; rdi = data, rsi = input_data, rdx = output_promise
DllCall:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov rbx, [rdi]          ; entry
    mov r14, [rdi + 8]      ; output_size
    mov r15, [rdi + 16]     ; inputMapLength
    mov r12, [rdi + 24]     ; call_stack_usage
    mov r13, [rdi + 32]     ; inputMap

    push rdx                ; save output_promise

    sub rsp, 6*8 ; space for temporary registers
    
    %ifdef DEBUG
    xor eax, eax
    mov [rsp], rax
    mov [rsp+8], rax
    mov [rsp+16], rax
    mov [rsp+24], rax
    mov [rsp+32], rax
    mov [rsp+40], rax
    %endif

    sub rsp, r12 ; stack

    ; stack
    ; [rsp]                - args
    ; [rsp + r12]          - temporary registers
    ; [rsp + r12 + 48]     - output_promise
    ; [rsp + r12 + 48 + 8] - rbp...

    xor r8d, r8d ; stack used
    xor r9d, r9d ; index
    xor r11d, r11d ; used registers
    lea rdi, [rsp + r12]

.arg_loop:
    cmp r9, r15
    jge .args_done

    shl r9, 5 ; todo fix this code
    mov r10, [r13 + r9 + 8] ; size
    shr r9, 5
    mov rax, [rsi + r9*8] ; pointer

    cmp r10, 16
    ja .to_stack

    cmp r10, 8
    jbe .try_register
    cmp r11, 5
    jae .to_stack
    mov rcx, [rax]
    mov [rdi + r11*8], rcx
    mov rcx, [rax + 8]
    mov [rdi + r11*8 + 8], rcx
    add r11, 2
    jmp .next_arg

.try_register:
    cmp r11, 6
    jge .to_stack
    mov rcx, [rax]
    mov [rdi + r11*8], rcx
    add r11, 1
    jmp .next_arg

.to_stack:
    push rdi
    push rsi
    lea rdi, [rsp + r8 + 16]
    mov rsi, rax
    mov rcx, r10
    add rcx, 7
    shr rcx, 3
    lea r8, [r8 + rcx * 8]
    rep movsq
    pop rsi
    pop rdi

.next_arg:
    inc r9
    jmp .arg_loop


.args_done:
    mov rdi, [rsp + r12]
    mov rsi, [rsp + r12 + 8]
    mov rdx, [rsp + r12 + 16]
    mov rcx, [rsp + r12 + 24]
    mov r8,  [rsp + r12 + 32]
    mov r9,  [rsp + r12 + 40]

    mov r13, [rsp + r12 + 48] ; load rdx - output promise

    call rbx

    ; save result
    test r13, r13
    jz .no_ret
    cmp r14, 1
    je .size1
    cmp r14, 2
    je .size2
    cmp r14, 4
    je .size4
    mov [r13], rax
    jmp .no_ret
.size1:
    mov [r13], al
    jmp .no_ret
.size2:
    mov [r13], ax
    jmp .no_ret
.size4:
    mov [r13], eax
.no_ret:

    lea rsp, [rsp + r12 + 48+8]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    mov rsp, rbp
    pop rbp
    ret
