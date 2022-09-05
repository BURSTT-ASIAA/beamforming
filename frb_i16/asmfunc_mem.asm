default rel

    section .data

bitMask:    db  16 dup 0xF0

    section .bss

align       32
realV:      resy 1
imagV:      resy 1

    section .text

    global asmfunc
asmfunc:
    push rbx
    push r15
    push rbp
    mov rbp, rsp

    ; load xmm15, xmm8
    vmovdqu xmm15, [bitMask]

    ; zeroed ymm13, ymm14(sum for square)
    vpxor ymm13, ymm13, ymm13
    vpxor ymm14, ymm14, ymm14
    xor r15, r15

    lea r10, [realV]
    lea r11, [imagV]

integralLp:
    ; unpack antenna data to realV(high 4 bits), imagV(low)
    vmovdqu xmm0, [rsi+r15]
    vmovdqu xmm1, xmm0

    pand xmm0, xmm15
    vpmovsxbw ymm0, xmm0
    vpsraw ymm0, ymm0, 4
    vmovdqa [r10], ymm0

    psllw xmm1, 4
    pand xmm1, xmm15
    vpmovsxbw ymm1, xmm1
    vpsraw ymm1, ymm1, 4
    vmovdqa [r11], ymm1

    ; clear ymm11 and ymm12 for real and imag parts
    xor rax, rax
    mov rbx, rdi
    vpxor ymm11, ymm11, ymm11
    vpxor ymm12, ymm12, ymm12
rowLp:
    ; load a column from matrix
    vmovdqa ymm2, [rbx]
    vmovdqa ymm3, [rbx+32]

    ; load a vector element
    vpbroadcastw ymm0, [r10+rax*2]
    vpbroadcastw ymm1, [r11+rax*2]

    ; calculate real part
    vpmullw ymm4, ymm0, ymm2
    vpaddsw ymm11, ymm11, ymm4
    vpmullw ymm4, ymm1, ymm3
    vpsubsw ymm11, ymm11, ymm4

    ; calculate imag part
    vpmullw ymm4, ymm0, ymm3
    vpaddsw ymm12, ymm12, ymm4
    vpmullw ymm4, ymm1, ymm2
    vpaddsw ymm12, ymm12, ymm4

    add rbx, 64
    inc rax
    cmp rax, 16
    jb rowLp

    ; integral
    vpmullw ymm0, ymm11, ymm11
    vpmulhw ymm1, ymm11, ymm11
    vpunpcklwd ymm2, ymm0, ymm1
    vpunpckhwd ymm3, ymm0, ymm1
    vpaddd ymm13, ymm13, ymm2
    vpaddd ymm14, ymm14, ymm3

    vpmullw ymm0, ymm12, ymm12
    vpmulhw ymm1, ymm12, ymm12
    vpunpcklwd ymm2, ymm0, ymm1
    vpunpckhwd ymm3, ymm0, ymm1
    vpaddd ymm13, ymm13, ymm2
    vpaddd ymm14, ymm14, ymm3

    add r15, 16
    dec rdx
    jnz integralLp

    ; rearrange
    vinserti128 ymm0, ymm13, xmm14, 1
    vextracti128 xmm2, ymm13, 1
    vinserti128 ymm1, ymm14, xmm2, 0

    ; convert to float
    vcvtdq2ps ymm0, ymm0
    vcvtdq2ps ymm1, ymm1

    ; copy to destination
    vmovaps [rcx], ymm0
    vmovaps [rcx+32], ymm1

    leave
    pop r15
    pop rbx
    ret
