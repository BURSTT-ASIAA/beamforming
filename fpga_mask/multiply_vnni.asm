default rel

    section .data

align       64
negImag:    times 16 dw 1, -1
swapReIm:   dw  1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14
            dw  17,16,19,18,21,20,23,22,25,24,27,26,29,28,31,30

    section .text

    global multiply_vnni
multiply_vnni:
    push rbp
    mov rbp, rsp

    ; local variables
    and rsp, -64
    sub rsp, 128

    ; clear dest
    vpxord zmm0, zmm0, zmm0
    vmovups [rcx], zmm0

    ; load zmm13, zmm12
    vmovdqu16 zmm13, [swapReIm]
    vmovdqu16 zmm12, [negImag]

    xor r8, r8
integralLp:
    vmovdqu16 zmm0, [rsi+r8]

    ; (imag, real)
    vpermw zmm1, zmm13, zmm0
    vmovdqu16 [rsp+64], zmm1

    ; (real, -imag)
    vpmullw zmm1, zmm12, zmm0
    vmovdqu16 [rsp], zmm1

    ; clear zmm10(re), zmm11(im)
    vpxord zmm10, zmm10, zmm10
    vpxord zmm11, zmm11, zmm11

    xor rax, rax
    mov r9, rdi
rowLp:
    ; load a column from matrix
    vmovdqu16 zmm0, [r9]

    ; calculate real part
    vpdpwssd zmm10, zmm0, [rsp+rax*4]{1to16}

    ; calculate imag part
    vpdpwssd zmm11, zmm0, [rsp+rax*4+64]{1to16}

    add r9, 64
    inc rax
    cmp rax, 16
    jb rowLp

    ; integral
    vcvtdq2ps zmm10, zmm10
    vcvtdq2ps zmm11, zmm11
    vmovups zmm0, [rcx]
    vfmadd231ps zmm0, zmm10, zmm10
    vfmadd231ps zmm0, zmm11, zmm11
    vmovups [rcx], zmm0

    add r8, 64
    dec rdx
    jnz integralLp

    leave
    ret
