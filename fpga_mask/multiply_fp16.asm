default rel

    section .data

align       64
evenOdd:    dw 0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30
            dw 1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31

    section .text

    global multiply_fp16
multiply_fp16:
    push rbp
    mov rbp, rsp

    ; local variables
    and rsp, -64
    sub rsp, 128

    ; clear dest
    vpxord zmm0, zmm0, zmm0
    vmovups [rcx], zmm0

    ; load zmm12
    vmovdqu16 zmm12, [evenOdd]

    vcvtsi2sh xmm13, rdx
    vpbroadcastw zmm13, xmm13
    mov r8, rsi
    mov r10, rdx
integralLp:
    ; clear zmm10
    vpxord zmm10, zmm10, zmm10

    xor rax, rax
    mov r9, rdi
rowLp:
    ; load a column from matrix
    vmovdqu16 zmm0, [r9]

    ; calculate product
    vfmaddcph zmm10, zmm0, [r8+rax*4]{1to16}

    add r9, 64
    inc rax
    cmp rax, 16
    jb rowLp

    ; integral
    vmulph zmm10, zmm10, zmm10
    vdivph zmm10, zmm10, zmm13
    vpermw zmm10, zmm12, zmm10
    vextractf32x8 ymm11, zmm10, 1

    vaddph ymm10, ymm10, [rcx]
    vaddph ymm10, ymm10, ymm11
    vmovups [rcx], ymm10

    add r8, 64
    dec r10
    jnz integralLp

    leave
    ret
