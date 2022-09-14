default rel

    section .text

    global asmfunc
asmfunc:
    push rbx
    push r15
    push rbp
    mov rbp, rsp

    mov r15, 100000
outerLp:
    vxorps zmm0, zmm0, zmm0
    vxorps zmm1, zmm0, zmm0
    vxorps zmm2, zmm0, zmm0
    vxorps zmm3, zmm0, zmm0
    vxorps zmm13, zmm0, zmm0
;    vxorps zmm11, zmm0, zmm0
;    vxorps zmm12, zmm0, zmm0
;    vxorps zmm9, zmm9, zmm9
;    vxorps zmm10, zmm10, zmm10
    xor rbx, rbx

integralLp:
    ; clear zmm11 and zmm12 for real and imag parts
    xor rax, rax
    vxorps zmm11, zmm11, zmm11
    vxorps zmm12, zmm12, zmm12
    vxorps zmm9, zmm9, zmm9
    vxorps zmm10, zmm10, zmm10

rowLp:
    ; calculate real part
    vfmadd231ps zmm9, zmm0, zmm2
    vfnmadd231ps zmm11, zmm1, zmm3

    ; calculate imag part
    vfmadd231ps zmm10, zmm0, zmm3
    vfmadd231ps zmm12, zmm1, zmm2

    inc rax
    cmp rax, 16
    jb rowLp

    ; integral
    vaddps zmm11, zmm11, zmm9
    vaddps zmm12, zmm12, zmm10
    vfmadd231ps zmm13, zmm11, zmm11
    vfmadd231ps zmm13, zmm12, zmm12

    inc rbx
    cmp rbx, 400
    jb integralLp

    ; copy to destination
    vmovaps [rcx], zmm13

    dec r15
    jnz outerLp

    leave
    pop r15
    pop rbx
    ret
