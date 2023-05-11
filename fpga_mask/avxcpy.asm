default rel

	section .text

	global avxcpy
avxcpy:
	push rbp
	mov rbp, rsp

	xor rax, rax
loop:
	vmovdqu16 zmm0, [rsi+rax]
	vmovdqu16 [rdi+rax], zmm0
	add rax, 64
	cmp rax, rdx
	jb loop

	leave
	ret

	global avxfill
avxfill:
	push rbp
	mov rbp, rsp

	xor rax, rax
loop2:
	vmovdqu16 zmm0, [rsi]
	vmovdqu16 [rdi+rax], zmm0
	add rax, 64
	cmp rax, rdx
	jb loop2

	leave
	ret
