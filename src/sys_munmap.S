.text

.globl sys_munmap
.hidden sys_munmap

.type sys_munmap, @function

sys_munmap:
	.cfi_startproc

	mov $0xb, %rax
	syscall
	ret

	.cfi_endproc

.size sys_munmap, .-sys_munmap

.section .note.GNU-stack,"",@progbits
