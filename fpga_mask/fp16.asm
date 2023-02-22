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
