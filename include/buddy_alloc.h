/*
 * Buddy Allocator
 * SPDX-License-Identifier: LGPL-3.0-only
 * Copyright (C) 2026 Rio
 */

#ifndef BUDDY_ALLOC_H
#define BUDDY_ALLOC_H

#include <stdint.h>

struct buddy_block_t;

void *blk_mmap(void);
void blk_munmap(struct buddy_block_t *blk);

void *buddy_alloc(struct buddy_block_t *blk, uint64_t order);
void buddy_free(struct buddy_block_t *blk, void *addr, uint64_t order);

#endif
