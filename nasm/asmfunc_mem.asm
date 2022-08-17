    section .data

swapReIm:   dw  1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14
            dw  17,16,19,18,21,20,23,22,25,24,27,26,29,28,31,30
evenOdd:    dd  0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15
allOne:     dd  8 dup 1.0
bitMask:    db  16 dup 0xF0

    section .bss


    section .text

    global asmfunc
asmfunc:
    push rbx
    push r15
    push rbp
    mov rbp, rsp

    ; load zmm15, xmm14, ymm8, zmm7, k2, k3
    vmovdqu16 zmm15, [swapReIm]
    vmovdqu8 xmm14, [bitMask]
    vmovups ymm8, [allOne]
    vmovdqu32 zmm7, [evenOdd]
    mov eax, 0x33
    kmovb k2, eax
    mov eax, 0xcc
    kmovb k3, eax

    ; zeroed zmm13(sum for square roots)
    vxorps zmm13, zmm13, zmm13
    xor r15, r15

integralLp:
    ; unpack antenna data to zmm0
    vmovdqu8 xmm0, [rsi+r15]
    vmovdqu8 xmm1, xmm0

    pand xmm0, xmm14
    vpmovsxbw ymm0, xmm0

    psllw xmm1, 4
    pand xmm1, xmm14
    vpmovsxbw ymm1, xmm1

    vpunpcklwd ymm2, ymm1, ymm0
    vpunpckhwd ymm3, ymm1, ymm0

    vpexpandq zmm0{k2}, zmm2
    vpexpandq zmm0{k3}, zmm3

    vpsraw zmm0, zmm0, 4

    ; set k1 opmask
    mov eax, 1
    kmovd k1, eax

    mov rax, 16
    mov rbx, rdi
rowLp:
    ; matrix calculation
    vmovdqu16 zmm16, [rbx]

    ; calculate real part (zmm11)
    vpmullw zmm1, zmm0, zmm16

    vextracti32x8 ymm2, zmm1, 1
    vpaddw ymm1, ymm1, ymm2
    vpmovsxwd zmm1, ymm1
 
    vpermd zmm2, zmm7, zmm1
    vextracti32x8 ymm1, zmm2, 1
    vpsubd ymm1, ymm2

    vextracti32x4 xmm2, ymm1, 1
    vpaddd xmm3, xmm1, xmm2

    ; calculate imaginary part (zmm12)
    vpermw zmm1, zmm15, zmm0
    vpmullw zmm1, zmm1, zmm16

    vextracti32x8 ymm2, zmm1, 1
    vpaddw ymm1, ymm1, ymm2
    vpmovsxwd zmm1, ymm1

    vextracti32x8 ymm2, zmm1, 1
    vpaddd ymm1, ymm1, ymm2
 
    vextracti32x4 xmm2, ymm1, 1
    paddd xmm1, xmm2

    ; merge real and imaginary then sum
    vinserti32x4 ymm1, ymm1, xmm3, 1

    vcvtdq2ps ymm1, ymm1
    vdpps ymm1, ymm1, ymm8, 0xF1
    vextractf32x4 xmm2, ymm1, 1

    ; save the sum
    vexpandps zmm11{k1}, zmm2
    vexpandps zmm12{k1}, zmm1

    ; shift opmask
    kshiftld k1, k1, 1

    add rbx, 64
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
