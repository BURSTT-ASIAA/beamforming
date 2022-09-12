default rel

    section .data

align       32
negImag:    times 8 dw 1, -1
bitMask:    times 16 db 0xF0

    section .bss

align       32
beam1:      resy 2
beam2:      resy 2

    section .text

    global asmfunc
asmfunc:
    push rbx
    push r15
    push rbp
    mov rbp, rsp

    ; load xmm15, ymm14, ymm13
    vmovdqa xmm15, [bitMask]
    vmovdqa ymm13, [negImag]

    ; clear ymm11, ymm12(sum for square)
    vxorps ymm12, ymm12, ymm12
    vxorps ymm11, ymm11, ymm11
    xor r15, r15

    ; more setup
    lea r10, [beam1]
    lea r11, [beam2]

integralLp:
    ; unpack antenna data to [beam]
    vmovdqa xmm0, [rsi+r15]
    vmovdqa xmm1, xmm0

    pand xmm0, xmm15
    psllw xmm1, 4
    pand xmm1, xmm15

    vpunpcklbw xmm2, xmm1, xmm0
    vpunpckhbw xmm3, xmm1, xmm0
    vpmovsxbw ymm0, xmm2
    vpmovsxbw ymm1, xmm3
    vpsraw ymm0, ymm0, 4
    vpsraw ymm1, ymm1, 4

    ; (imag, real)
    vpshuflw ymm2, ymm0, 0xb1
    vpshufhw ymm2, ymm2, 0xb1
    vmovdqa [r10], ymm2
    vpshuflw ymm2, ymm1, 0xb1
    vpshufhw ymm2, ymm2, 0xb1
    vmovdqa [r10+32], ymm2

    ; (real, -imag)
    vpmullw ymm2, ymm0, ymm13
    vmovdqa [r11], ymm2
    vpmullw ymm2, ymm1, ymm13
    vmovdqa [r11+32], ymm2

    ; clear ymm7, ymm8(re), ymm9, ymm10(im)
    vpxor ymm7, ymm7, ymm7
    vmovdqa ymm8, ymm7
    vmovdqa ymm9, ymm7
    vmovdqa ymm10, ymm7

    xor rax, rax
    mov rbx, rdi
rowLp:
    ; load a column from matrix
    vmovdqa ymm0, [rbx]
    vmovdqa ymm1, [rbx+32]

    ; load a vector element
    vpbroadcastd ymm2, [r10+rax*4]
    vpbroadcastd ymm3, [r11+rax*4]

    ; calculate real part
    vpmaddwd ymm4, ymm0, ymm3
    vpaddd ymm7, ymm7, ymm4
    vpmaddwd ymm4, ymm1, ymm3
    vpaddd ymm8, ymm8, ymm4

    ; calculate imag part
    vpmaddwd ymm4, ymm0, ymm2
    vpaddd ymm9, ymm9, ymm4
    vpmaddwd ymm4, ymm1, ymm2
    vpaddd ymm10, ymm10, ymm4

    add rbx, 64
    inc rax
    cmp rax, 16
    jb rowLp

    ; integral
    vcvtdq2ps ymm7, ymm7
    vcvtdq2ps ymm8, ymm8
    vfmadd231ps ymm11, ymm7, ymm7
    vfmadd231ps ymm12, ymm8, ymm8

    vcvtdq2ps ymm9, ymm9
    vcvtdq2ps ymm10, ymm10
    vfmadd231ps ymm11, ymm9, ymm9
    vfmadd231ps ymm12, ymm10, ymm10

    add r15, 16
    dec rdx
    jnz integralLp

    ; copy to destination
    vmovaps [rcx], ymm11
    vmovaps [rcx+32], ymm12

    leave
    pop r15
    pop rbx
    ret
