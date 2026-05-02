# Buddy Allocator

A userspace buddy memory allocator written in C, designed to be used as a backend for more general allocators.

Manages memory using the binary buddy allocation algorithm.

---

## Build

Targets:
- `shared_lib` (default)
- `static_lib`
- `test`
- `debug`
- `verbose_debug`

Debug targets also build the test target.

Example:
```bash
make debug
```

---

## Usage

```c
#include "buddy_alloc.h"

/*
 * Create allocation arena with `mmap`.
 * The allocator uses this memory region for all allocations and storing metadata.
 */
void *blk = blk_mmap();

/*
 * Allocate memory from the buddy allocator
 * `order` defines size as: 4096 << order bytes
 */
void *p = buddy_alloc(blk, 12);

/*
 * Free memory allocated with buddy_alloc()
 */
buddy_free(blk, p, 12);

/*
 * Deallocate mmaped allocation arena.
 */
blk_munmap(blk);
```

---

## License

This project is licensed under the GNU Lesser General Public License v3.0.

See LICENSE for details.
