	section .text

	global f16convert
f16convert:
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
