.text

.globl sys_mmap
.hidden sys_mmap

.type sys_mmap, @function

sys_mmap:
	.cfi_startproc

	mov $0x9, %rax
	mov %rcx, %r10
	syscall
	ret

	.cfi_endproc

.size sys_mmap, .-sys_mmap

.section .note.GNU-stack,"",@progbits
