    section .data

allOne:     times 4 dd 1.0
bitMask:    times 16 db 0xF0

    section .bss


    section .text

    global asmfunc
asmfunc:
    push rbx
    push r15
    push rbp
    mov rbp, rsp

    ; load xmm14, xmm8
    vmovdqu8 xmm14, [bitMask]
    vmovups xmm8, [allOne]

    ; zeroed zmm13(sum for square roots)
    vxorps zmm13, zmm13, zmm13
    xor r15, r15

integralLp:
    ; unpack antenna data to zmm0(high 4 bits), zmm1(low)
    vmovdqu8 xmm0, [rsi+r15]
    vmovdqu8 xmm1, xmm0

    pand xmm0, xmm14
    vpmovsxbd zmm0, xmm0
    vpsrad zmm0, zmm0, 4
    vcvtdq2ps zmm0, zmm0

    psllw xmm1, 4
    pand xmm1, xmm14
    vpmovsxbd zmm1, xmm1
    vpsrad zmm1, zmm1, 4
    vcvtdq2ps zmm1, zmm1

    ; set k1 opmask
    mov eax, 1
    kmovd k1, eax

    mov rax, 16
    mov rbx, rdi
rowLp:
    ; load a row from matrix
    vmovdqu16 zmm16, [rbx]
    vmovdqu16 zmm17, [rbx+64]

    ; calculate real part (zmm11)
    vmulps zmm2, zmm1, zmm17
    vfmsub231ps zmm2, zmm0, zmm16
    vextractf32x8 ymm3, zmm2, 1
    vaddps ymm2, ymm2, ymm3
    vextractf32x4 xmm3, ymm2, 1
    vaddps xmm2, xmm2, xmm3
    dpps xmm2, xmm8, 0xF1
    vexpandps zmm11{k1}, zmm2

    ; calculate imaginary part (zmm12)
    vmulps zmm2, zmm0, zmm17
    vfmadd231ps zmm2, zmm1, zmm16
    vextractf32x8 ymm3, zmm2, 1
    vaddps ymm2, ymm2, ymm3
    vextractf32x4 xmm3, ymm2, 1
    vaddps xmm2, xmm2, xmm3
    dpps xmm2, xmm8, 0xF1
    vexpandps zmm12{k1}, zmm2

    ; shift opmask
    kshiftld k1, k1, 1

    add rbx, 128
    dec rax
    jnz rowLp

    ; calculate square sum
    vmulps zmm11, zmm11, zmm11
    vmulps zmm12, zmm12, zmm12
    vaddps zmm11, zmm11, zmm12

    ; integral
    vaddps zmm13, zmm13, zmm11
    add r15, 16
    dec rdx
    jnz integralLp

    ; copy to destination
    vmovaps [rcx], zmm13

    leave
    pop r15
    pop rbx
    ret
