default rel

    section .data

align       64
negImag:    times 16 dw 1, -1
swapReIm:   dw  1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14
            dw  17,16,19,18,21,20,23,22,25,24,27,26,29,28,31,30
bitMask:    times 16 dd 0xF0
;minFloat:   dd 0x38800000   ; smallest positive normal number for FP16

    section .text

    global asmfunc
asmfunc:
    push rbx
    push r15
    push r14
    push r13
    push r12
    push rbp
    mov rbp, rsp

    ; local variables
    and rsp, -64
    sub rsp, 128
    lea r10, [rsp+64]
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

    xor r15, r15
    mov r14, rdx

integralLp3:
    xor r13, r13
    mov rbx, rdi
;    prefetchnta [rdi]
;    prefetchnta [rsi+r15+16384]

integralLp2:
    mov r12, 4
    vmovdqa32 zmm14, [rsi+r15]

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
    vmovdqu16 [r10], zmm1

    ; (real, -imag)
    vpmullw zmm1, zmm0, zmm12
    vmovdqu16 [r11], zmm1

    ; clear zmm10(re), zmm11(im)
    vpxord zmm10, zmm10, zmm10
    vpxord zmm11, zmm11, zmm11

    xor rax, rax
;    mov rbx, rdi
rowLp:
    ; load a column from matrix
    vmovdqu16 zmm0, [rbx]

    ; calculate real part
    vpdpwssd zmm10, zmm0, [r11+rax*4]{1to16}

    ; calculate imag part
    vpdpwssd zmm11, zmm0, [r10+rax*4]{1to16}

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
    cmp r13, rcx
    jb integralLp2

    mov rax, rcx
    shl rax, 4
    add r15, 16384
    sub r15, rax
    dec r14
    jnz integralLp3

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
    pop r12
    pop r13
    pop r14
    pop r15
    pop rbx
    ret
