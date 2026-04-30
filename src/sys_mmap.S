.global sys_mmap

# C function call:
#	args -> rdi, rsi, rdx, rcx, r8, r9
# syscall:
#	args -> rdi, rsi, rdx, r10, r8, r9
sys_mmap:
	mov $0x9, %rax
	mov %rcx, %r10
	syscall
	ret

.section .note.GNU-stack,"",@progbits
