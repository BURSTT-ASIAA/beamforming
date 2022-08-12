    extern printf
    global main

section .data

HelloMsg    db "CPUID: %x, %x, %x, %d", 10, 0

section .bss

section .text

main:
    push rbx
    push rbp
    mov rbp, rsp
    sub rsp, 8

    mov eax, 7
    mov ecx, 0
    cpuid


    mov rdi, HelloMsg
    mov esi, ebx
    mov r8d, eax
    call printf

    mov rsp, rbp
    pop rbp
    pop rbx
    ret
