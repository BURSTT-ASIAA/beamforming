default rel

    section .data

align       64
negImag:    times 16 dw 1, -1
swapReIm:   dw  1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14
            dw  17,16,19,18,21,20,23,22,25,24,27,26,29,28,31,30
bitMask:    times 16 db 0xF0

    section .bss

align       64
beam1:      resz 1
beam2:      resz 1

    section .text

    global asmfunc
asmfunc:
    push rbx
    push r15
    push rbp
    mov rbp, rsp

    ; load xmm15, zmm13, zmm12
    vmovdqu8 xmm15, [bitMask]
    vmovdqu16 zmm13, [swapReIm]
    vmovdqu16 zmm12, [negImag]

    ; clear zmm14(sum for square)
    vxorps zmm14, zmm14, zmm14
    xor r15, r15

    ; more setup
    lea r10, [beam1]
    lea r11, [beam2]

integralLp:
    ; unpack antenna data to [beam]
    vmovdqu8 xmm0, [rsi+r15]
    vmovdqu8 xmm1, xmm0

    pand xmm0, xmm15
    psllw xmm1, 4
    pand xmm1, xmm15

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
    mov rbx, rdi
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
    vfmadd231ps zmm14, zmm10, zmm10
    vfmadd231ps zmm14, zmm11, zmm11

    add r15, 16
    dec rdx
    jnz integralLp

    ; copy to destination
    vmovaps [rcx], zmm14

    leave
    pop r15
    pop rbx
    ret
