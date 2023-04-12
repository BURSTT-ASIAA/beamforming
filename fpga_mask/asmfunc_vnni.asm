%define NR_2C 8
;%define DATA_OFF (8192 + 64) * 2
%define DATA_OFF 8192 * 2
%define mask [r11+128]
%define cur_mask [r11+136]
%define bmask [r11+144]
%define cur_bmask [r11+152]
%define voltage_ch [r11+160]
%define voltage_data [r11+176]
%define pkt_count [r11+184]
%define parm1 [rbp+80]
%define parm2 [rbp+88]
default rel

    section .data

align       64
negImag:    times 16 dw 1, -1
swapReIm:   dw  1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14
            dw  17,16,19,18,21,20,23,22,25,24,27,26,29,28,31,30
bitMask:    times 16 dw 0xF0
pickOdd:	dw 1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31
scaling:    dd 2097152.0  ; 4096**2 / (128/16)

    section .text

    global asmfunc
asmfunc:
    push rbx
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push rbp
    mov rbp, rsp

    ; local variables
    and rsp, -64
    sub rsp, 192
    lea r11, [rsp]
    mov rax, rcx
    shl rax, 6
    sub rsp, rax

    ; load data mask
    xor rax, rax
    inc rax
    shr r9, 1
    jnc evenPkg
    shl rax, 1
evenPkg:
    mov bmask, rax
    shl r9, 1
    mov mask, r9

    ; load ymm15, zmm13, zmm12
    vmovdqa32 ymm15, [bitMask]
    vmovdqu16 zmm13, [swapReIm]
    vmovdqu16 zmm12, [negImag]

    ; clear (sum for square)
    vxorps zmm0, zmm0, zmm0

    ; calculate voltage index
    movdqa voltage_ch, xmm0
    mov rax, parm1
    add rax, 16
    shl rax, 32
    add rax, parm1
    mov voltage_ch, rax

    xor rax, rax
    mov pkt_count, rax
    mov rbx, rcx
clearLp:
    vmovaps [rsp+rax], zmm0
    add rax, 64
    dec rbx
    jnz clearLp

    xor r9, r9      ; starting channel

integralLp4:
    mov r14, rdx    ; nr_packet
    mov r15, r9
    shl r15, 4      ; data offset
    mov rax, mask
    mov cur_mask, rax
    mov rax, bmask
    mov cur_bmask, rax

integralLp3:
    mov rax, cur_mask
    mov rax, [rax]
    and rax, cur_bmask
    jz passPkt

    inc qword pkt_count
    mov r13, r9     ; current channel
    mov rbx, r9
    shl rbx, 10
    add rbx, rdi    ; matrix pointer
    mov r10, NR_2C

integralLp2:
    mov r12, 2
    vmovdqa32 ymm14, [rsi+r15]
    prefetchnta [rsi+r15+DATA_OFF]

integralLp:
    ; unpack antenna data to [beam]
    vpand ymm0, ymm14, ymm15
    vpmovwb xmm0, ymm0

    vpsllw ymm1, ymm14, 4
    vpand ymm1, ymm1, ymm15
    vpmovwb xmm1, ymm1

    vpunpcklbw xmm2, xmm1, xmm0
    vpunpckhbw xmm3, xmm1, xmm0
    vinserti32x4 ymm0, ymm2, xmm3, 1
    vpmovsxbw zmm0, ymm0
    vpsraw zmm0, zmm0, 4

    ; (imag, real)
    vpermw zmm1, zmm13, zmm0
    vmovdqu16 [r11+64], zmm1

    ; (real, -imag)
    vpmullw zmm1, zmm0, zmm12
    vmovdqu16 [r11], zmm1

    ; clear zmm10(re), zmm11(im)
    vpxord zmm10, zmm10, zmm10
    vpxord zmm11, zmm11, zmm11

    xor rax, rax
rowLp:
    ; load a column from matrix
    vmovdqu16 zmm0, [rbx]

    ; calculate real part
    vpdpwssd zmm10, zmm0, [r11+rax*4]{1to16}

    ; calculate imag part
    vpdpwssd zmm11, zmm0, [r11+rax*4+64]{1to16}

    add rbx, 64
    inc rax
    cmp rax, 16
    jb rowLp

    ; store voltage data
    mov rax, parm1
    cmp rax, 0
    jl noBeams
    ; calculating
    vxorps zmm0, zmm0, zmm0
    vmovdqa32 xmm0, voltage_ch
    vpermi2d zmm0, zmm10, zmm11
    vpmovsdw voltage_data, xmm0
    shr dword voltage_data, 12
    mov eax, voltage_data
    and byte voltage_data, 0x0f
    shr eax, 12
    and al, 0xf0
    or al, voltage_data
    ; one byte data
    push rbx
    mov rbx, parm2
    mov [rbx], al
    pop rbx
    inc qword parm2

noBeams:
    ; integral
    vcvtdq2ps zmm10, zmm10
    vcvtdq2ps zmm11, zmm11
    mov rax, r13
    shl rax, 6
    vmovaps zmm0, [rsp+rax]
    vfmadd231ps zmm0, zmm10, zmm10
    vfmadd231ps zmm0, zmm11, zmm11
    vmovaps [rsp+rax], zmm0

    vpsrldq ymm14, ymm14, 1
    inc r13
    dec r12
    jnz integralLp

    add r15, 32
    dec r10
    jnz integralLp2

    sub r15, NR_2C * 32
    jmp noBeam

passPkt:
    ; store zero voltage data
    mov rax, parm1
    cmp rax, 0
    jl noBeam
    ; assume NR_2C=8, 16 bytes
    pxor xmm0, xmm0
    mov rax, parm2
    movdqa [rax], xmm0
    add qword parm2, NR_2C * 2

noBeam:
    mov rax, cur_bmask
    mov rbx, rax
    shl rax, 2
    and rax, rax
    jnz passMask
    mov rax, cur_mask
    add rax, 8
    mov cur_mask, rax
    shr rbx, 62
    mov rax, rbx
passMask:
    mov cur_bmask, rax

    add qword parm2, 1024 -  NR_2C * 2
    add r15, DATA_OFF
    dec r14
    jnz integralLp3

    mov rax, rdx
    shl rax, 10
    sub rax, NR_2C * 2
    sub qword parm2, rax
    add r9, NR_2C * 2
    cmp r9, rcx
    jb integralLp4

    ; normalization
    vxorps zmm2, zmm2, zmm2
    mov rax, pkt_count
    and rax, rax
    jz allzero
    vpbroadcastd zmm2, pkt_count
    vcvtdq2ps zmm2, zmm2
    vmulps zmm2, zmm2, [scaling]{1to16}
    vrcp14ps zmm2, zmm2
allzero:

    ; copy to destination
    xor rax, rax
    mov rbx, rcx
    vpxorq zmm1, zmm1, zmm1
    vmovdqu16 ymm1, [pickOdd]

copyLp:
    vmovaps zmm3, [rsp+rax*2]
    vmulps zmm3, zmm3, zmm2
    vpermw zmm0, zmm1, zmm3
;    vpermw zmm0, zmm1, [rsp+rax*2]
    vmovdqu16 [r8+rax], ymm0
    add rax, 32
    dec rbx
    jnz copyLp

    leave
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    pop rbx
    ret
