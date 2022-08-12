    section .data

swapReIm:   dw 1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14
            dw 17,16,19,18,21,20,23,22,25,24,27,26,29,28,31,30
bitMask:    db 16 dup 0xF0

    section .bss

sum:        resz    16

    section .text

%macro  dotproduct 1

    ; calculate real part (zmm11)
    vpmullw zmm1, zmm0, %1
    vextracti32x8 ymm2, zmm1, 1

;    vpmovsxwd zmm1, ymm1
;    vpmovsxwd zmm2, ymm2
;    vpaddd zmm1, zmm1, zmm2
    vpaddw ymm1, ymm1, ymm2
    vpmovsxwd zmm1, ymm1
 
    vextracti32x8 ymm2, zmm1, 1
    vpaddd ymm1, ymm1, ymm2
    vextracti32x4 xmm2, ymm1, 1
    phsubd xmm1, xmm2
    phaddd xmm1, xmm1
    phaddd xmm1, xmm1
    vpexpandd zmm11{k1}, zmm1

    ; calculate imaginary part (zmm12)
    vpermw zmm1, zmm15, zmm0
    vpmullw zmm1, zmm1, %1
    vextracti32x8 ymm2, zmm1, 1

;    vpmovsxwd zmm1, ymm1
;    vpmovsxwd zmm2, ymm2
;    vpaddd zmm1, zmm1, zmm2
    vpaddw ymm1, ymm1, ymm2
    vpmovsxwd zmm1, ymm1

    vextracti32x8 ymm2, zmm1, 1
    vpaddd ymm1, ymm1, ymm2
    vextracti32x4 xmm2, ymm1, 1
    phaddd xmm1, xmm2
    phaddd xmm1, xmm1
    phaddd xmm1, xmm1
    vpexpandd zmm12{k1}, zmm1

    ; shift opmask
    kshiftld k1, k1, 1

%endmacro

    global asmfunc
asmfunc:
    push rbx
    push r15
    push rbp
    mov rbp, rsp

    ; load zmm15, xmm14, k2, k3
    vmovdqu16 zmm15, [swapReIm]
    vmovdqu8 xmm14, [bitMask]
    mov eax, 0x33
    kmovb k2, eax
    mov eax, 0xcc
    kmovb k3, eax

    ; load matrix to zmm16 - zmm31
    vmovdqu16 zmm16, [rdi]
    vmovdqu16 zmm17, [rdi+64]
    vmovdqu16 zmm18, [rdi+64*2]
    vmovdqu16 zmm19, [rdi+64*3]
    vmovdqu16 zmm20, [rdi+64*4]
    vmovdqu16 zmm21, [rdi+64*5]
    vmovdqu16 zmm22, [rdi+64*6]
    vmovdqu16 zmm23, [rdi+64*7]
    vmovdqu16 zmm24, [rdi+64*8]
    vmovdqu16 zmm25, [rdi+64*9]
    vmovdqu16 zmm26, [rdi+64*10]
    vmovdqu16 zmm27, [rdi+64*11]
    vmovdqu16 zmm28, [rdi+64*12]
    vmovdqu16 zmm29, [rdi+64*13]
    vmovdqu16 zmm30, [rdi+64*14]
    vmovdqu16 zmm31, [rdi+64*15]

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

    ; matrix calculation
    dotproduct zmm16
    dotproduct zmm17
    dotproduct zmm18
    dotproduct zmm19
    dotproduct zmm20
    dotproduct zmm21
    dotproduct zmm22
    dotproduct zmm23
    dotproduct zmm24
    dotproduct zmm25
    dotproduct zmm26
    dotproduct zmm27
    dotproduct zmm28
    dotproduct zmm29
    dotproduct zmm30
    dotproduct zmm31

    ; calculate square root
    vcvtdq2ps zmm11, zmm11
    vmulps zmm11, zmm11, zmm11
    vcvtdq2ps zmm12, zmm12
    vmulps zmm12, zmm12, zmm12
    vaddps zmm11, zmm11, zmm12
    vsqrtps zmm11, zmm11

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
