%define NR_4C 4
%define DATA_OFF (8192 + 64) * 2
default rel

    section .data

align       64
negImag:    times 16 dw 1, -1
swapReIm:   dw  1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14
            dw  17,16,19,18,21,20,23,22,25,24,27,26,29,28,31,30
bitMask:    times 16 dd 0xF0

    section .text

    global asmfunc
asmfunc:
    push rbx
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push rbp
    mov rbp, rsp

    ; local variables
    and rsp, -64
    sub rsp, 128
    lea r11, [rsp]
    mov rax, rcx
    shl rax, 6
    sub rsp, rax

    ; load zmm15, zmm13, zmm12
    vmovdqa32 zmm15, [bitMask]
    vmovdqu16 zmm13, [swapReIm]
    vmovdqu16 zmm12, [negImag]

    ; clear (sum for square)
    vxorps zmm0, zmm0, zmm0
    xor rax, rax
    mov rbx, rcx

clearLp:
    vmovaps [rsp+rax], zmm0
    add rax, 64
    dec rbx
    jnz clearLp

    xor r9, r9      ; starting channel

integralLp4:
    mov r14, rdx    ; nr_packet
    mov r15, r9
    shl r15, 4      ; data offset

integralLp3:
    mov r13, r9     ; current channel
    mov rbx, r9
    shl rbx, 10
    add rbx, rdi    ; matrix pointer
    mov r10, NR_4C

integralLp2:
    mov r12, 4
    vmovdqa32 zmm14, [rsi+r15]
    prefetchnta [rsi+r15+DATA_OFF]

integralLp:
    ; unpack antenna data to [beam]
    vpandd zmm0, zmm14, zmm15
    vpmovdb xmm0, zmm0

    vpslld zmm1, zmm14, 4
    vpandd zmm1, zmm1, zmm15
    vpmovdb xmm1, zmm1

    vpunpcklbw xmm2, xmm1, xmm0
    vpunpckhbw xmm3, xmm1, xmm0
    vinserti32x4 ymm0, ymm2, xmm3, 1
    vpmovsxbw zmm0, ymm0
    vpsraw zmm0, zmm0, 4

    ; (imag, real)
    vpermw zmm1, zmm13, zmm0
    vmovdqu16 [r11+64], zmm1

    ; (real, -imag)
    vpmullw zmm1, zmm0, zmm12
    vmovdqu16 [r11], zmm1

    ; clear zmm10(re), zmm11(im)
    vpxord zmm10, zmm10, zmm10
    vpxord zmm11, zmm11, zmm11

    xor rax, rax

rowLp:
    ; load a column from matrix
    vmovdqu16 zmm0, [rbx]

    ; calculate real part
    vpdpwssd zmm10, zmm0, [r11+rax*4]{1to16}

    ; calculate imag part
    vpdpwssd zmm11, zmm0, [r11+rax*4+64]{1to16}

    add rbx, 64
    inc rax
    cmp rax, 16
    jb rowLp

    ; integral
    vcvtdq2ps zmm10, zmm10
    vcvtdq2ps zmm11, zmm11
    mov rax, r13
    shl rax, 6
    vmovaps zmm0, [rsp+rax]
    vfmadd231ps zmm0, zmm10, zmm10
    vfmadd231ps zmm0, zmm11, zmm11
    vmovaps [rsp+rax], zmm0

    vpsrldq zmm14, zmm14, 1
    inc r13
    dec r12
    jnz integralLp

    add r15, 64
    dec r10
    jnz integralLp2

    add r15, DATA_OFF - NR_4C * 64
    dec r14
    jnz integralLp3

    add r9, NR_4C * 4
    cmp r9, rcx
    jb integralLp4

    ; copy to destination
    xor rax, rax
    mov rbx, rcx

copyLp:
    vmovaps zmm0, [rsp+rax]
    vmovaps [r8+rax], zmm0
    add rax, 64
    dec rbx
    jnz copyLp

    leave
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    pop rbx
    ret
