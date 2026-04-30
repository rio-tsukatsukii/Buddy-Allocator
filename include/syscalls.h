#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>

extern long sys_mmap(uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags,
		uint64_t fd, uint64_t off);

extern long sys_munmap(uint64_t addr, uint64_t len);

#endif
