.global sys_munmap
.hidden sys_munmap

sys_munmap:
	mov $0xb, %rax
	syscall
	ret

.section .note.GNU-stack,"",@progbits
