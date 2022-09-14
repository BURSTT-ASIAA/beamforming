default rel

    section .data

bitMask:    times 16 db 0xF0

    section .bss

align       64
realV:      resz 1
imagV:      resz 1

    section .text

    global asmfunc
asmfunc:
    push rbx
    push r15
    push rbp
    mov rbp, rsp

    ; load xmm15, xmm8
    vmovdqu8 xmm15, [bitMask]

    ; zeroed zmm13(sum for square)
    vxorps zmm13, zmm13, zmm13
    xor r15, r15

    lea r10, [realV]
    lea r11, [imagV]

integralLp:
    ; unpack antenna data to realV(high 4 bits), imagV(low)
    vmovdqu8 xmm0, [rsi+r15]
    vmovdqu8 xmm1, xmm0

    pand xmm0, xmm15
    vpmovsxbd zmm0, xmm0
    vpsrad zmm0, zmm0, 4
    vcvtdq2ps zmm0, zmm0
    vmovaps [r10], zmm0

    psllw xmm1, 4
    pand xmm1, xmm15
    vpmovsxbd zmm1, xmm1
    vpsrad zmm1, zmm1, 4
    vcvtdq2ps zmm1, zmm1
    vmovaps [r11], zmm1

    ; clear zmm11 and zmm12 for real and imag parts
    xor rax, rax
    mov rbx, rdi
    vxorps zmm11, zmm11, zmm11
    vxorps zmm12, zmm12, zmm12
;    vxorps zmm9, zmm9, zmm9
;    vxorps zmm10, zmm10, zmm10

rowLp:
    ; load a column from matrix
    vmovaps zmm2, [rbx]
    vmovaps zmm3, [rbx+64]

    ; load a vector element
    vbroadcastss zmm0, [r10+rax*4]
    vbroadcastss zmm1, [r11+rax*4]

    ; calculate real part
    vfmadd231ps zmm11, zmm0, zmm2
    vfnmadd231ps zmm11, zmm1, zmm3
;    vmulps zmm10, zmm0, zmm2
;    vaddps zmm11, zmm11, zmm10
;    vmulps zmm10, zmm1, zmm3
;    vsubps zmm11, zmm11, zmm10

    ; calculate imag part
    vfmadd231ps zmm12, zmm0, zmm3
    vfmadd231ps zmm12, zmm1, zmm2

    add rbx, 128
    inc rax
    cmp rax, 16
    jb rowLp

    ; integral
;    vaddps zmm11, zmm11, zmm9
;    vaddps zmm12, zmm12, zmm10
    vfmadd231ps zmm13, zmm11, zmm11
    vfmadd231ps zmm13, zmm12, zmm12

    add r15, 16
    dec rdx
    jnz integralLp

    ; copy to destination
    vmovaps [rcx], zmm13

    leave
    pop r15
    pop rbx
    ret
