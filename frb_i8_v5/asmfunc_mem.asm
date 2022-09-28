default rel

    section .data

align       32
negImag:    times 16 db 1, -1
swapReIm:   db  1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14
            db  1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14
bitMask:    times 16 db 0xF0

    section .bss

align       32
beam1:      resy 1
beam2:      resy 1

    section .text

    global asmfunc
asmfunc:
    push rbx
    push r15
    push rbp
    mov rbp, rsp

    ; load xmm15, ymm14, ymm13
    vmovdqa xmm15, [bitMask]
    vmovdqa ymm14, [swapReIm]
    vmovdqa ymm13, [negImag]

    ; clear zmm12(sum for square)
    vxorps zmm12, zmm12, zmm12
    xor r15, r15

    ; more setup
    lea r10, [beam1]
    lea r11, [beam2]

integralLp:
    ; unpack antenna data to [beam]
    vmovdqu xmm0, [rsi+r15]
    vmovdqu xmm1, xmm0

    pand xmm0, xmm15
    psllw xmm1, 4
    pand xmm1, xmm15

    vpunpcklbw xmm2, xmm1, xmm0
    vpunpckhbw xmm3, xmm1, xmm0
    vinserti128 ymm0, ymm2, xmm3, 1
    vpmovsxbw zmm0, ymm0
    vpsraw zmm0, zmm0, 4
    vpmovwb ymm0, zmm0

    ; (imag, real)
    vpshufb ymm1, ymm0, ymm14
    vmovdqa [r10], ymm1

    ; (real, -imag)
    vpsignb ymm1, ymm0, ymm13
    vmovdqa [r11], ymm1

    ; clear ymm10(re), ymm11(im)
    vpxor ymm10, ymm10, ymm10
    vpxor ymm11, ymm11, ymm11

    xor rax, rax
    mov rbx, rdi
rowLp:
    ; load a column from matrix
    vmovdqa ymm0, [rbx]
    vpsignb ymm5, ymm0, ymm0

    ; load a vector element
    vpbroadcastw ymm1, [r10+rax*2]
    vpbroadcastw ymm2, [r11+rax*2]

    ; calculate real part
    vpsignb ymm2, ymm2, ymm0
    vpmaddubsw ymm3, ymm5, ymm2

    ; calculate imag part
    vpsignb ymm1, ymm1, ymm0
    vpmaddubsw ymm4, ymm5, ymm1

    ; accumulate
    vpaddw ymm10, ymm10, ymm3
    vpaddw ymm11, ymm11, ymm4

    add rbx, 32
    inc rax
    cmp rax, 16
    jb rowLp

    ; integral
    vpmovsxwd zmm10, ymm10
    vcvtdq2ps zmm10, zmm10
    vfmadd231ps zmm12, zmm10, zmm10

    vpmovsxwd zmm11, ymm11
    vcvtdq2ps zmm11, zmm11
    vfmadd231ps zmm12, zmm11, zmm11

    add r15, 16
    dec rdx
    jnz integralLp

    ; copy to destination
    vmovaps [rcx], zmm12

    leave
    pop r15
    pop rbx
    ret
