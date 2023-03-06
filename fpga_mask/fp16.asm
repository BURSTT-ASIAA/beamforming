	section .text

	global f32tof16
f32tof16:
	push rbp
	mov rbp, rsp

	; convert 1024x16 floats
	xor rax, rax
loop:
	vmovaps zmm0, [rsi+rax*2]
	vcvtps2ph [rdi+rax], zmm0, 0
	add rax, 32
	cmp rax, 1024 * 32
	jb loop

	leave
	ret

	global f16tof32
f16tof32:
	push rbp
	mov rbp, rsp

	; convert 1024x16 floats
	xor rax, rax
loop2:
	vcvtph2ps zmm0, [rsi+rax]
	vmovaps [rdi+rax*2], zmm0
	add rax, 32
	cmp rax, 1024 * 32
	jb loop2

	leave
	ret

	global f32tob16
f32tob16:
	push rbp
	mov rbp, rsp

	; convert 1024x16 brain floats
	xor rax, rax
loop3:
	vmovaps zmm0, [rsi+rax*2]
	vmovaps zmm1, [rsi+rax*2+64]
	vcvtne2ps2bf16 zmm0, zmm1, zmm0
	vmovdqu16 [rdi+rax], zmm0
	add rax, 64
	cmp rax, 1024 * 32
	jb loop3

	leave
	ret

	global b16tof32
b16tof32:
	push rbp
	mov rbp, rsp

	; convert 1024x16 floats
	xor rax, rax
	vpxor xmm1, xmm1, xmm1
loop4:
	vmovdqa xmm2, [rsi+rax]
	vpunpcklwd xmm0, xmm1, xmm2
	movaps [rsi+rax*2], xmm0
	vpunpckhwd xmm0, xmm1, xmm2
	movaps [rsi+rax*2+16], xmm0
	add rax, 16
	cmp rax, 1024 * 32
	jb loop4

	leave
	ret
