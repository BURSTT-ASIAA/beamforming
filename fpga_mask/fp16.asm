default rel

	section .data

pickOdd:	dw 1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31

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
	vpxorq zmm1, zmm1, zmm1
	vmovdqu16 ymm1, [pickOdd]
loop3:
	vpermw zmm0, zmm1, [rsi+rax*2]
	vmovdqu16 [rdi+rax], ymm0
	add rax, 32
	cmp rax, 1024 * 32
	jb loop3

	leave
	ret

	global b16tof32
b16tof32:
	push rbp
	mov rbp, rsp

	; convert 1024x16 brain floats
	xor rax, rax
	vpxor xmm1, xmm1, xmm1
loop4:
	vmovdqa xmm2, [rsi+rax]
	vpunpcklwd xmm0, xmm1, xmm2
	movaps [rdi+rax*2], xmm0
	vpunpckhwd xmm0, xmm1, xmm2
	movaps [rdi+rax*2+16], xmm0
	add rax, 16
	cmp rax, 1024 * 32
	jb loop4

	leave
	ret
