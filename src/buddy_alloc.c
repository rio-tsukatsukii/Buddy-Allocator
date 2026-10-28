#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <sys/mman.h>

#include "buddy_alloc.h"

#define PAGE_SIZE		0x1000

#define BLK_HEADER_SIZE		0x19000
#define ALLOCABLE_BLK_SIZE	0x40000000

#define SPLIT_BITMAP_NODES	0x3ffff
#define FREE_BITMAP_NODES	0x7ffff

#define SPLIT_BITMAP_SIZE	0x1000
#define FREE_BITMAP_SIZE	0x2000

#define MAX_ORDER		0x13

struct buddy_chunk_t {
	struct buddy_chunk_t *next;
};

struct buddy_free_list_t {
	struct buddy_chunk_t *head;
};

/*
  Block Header Structure
+--------------------------------+
| Block Base Address             | 0x0-0x8
 --------------------------------
| Free List - 0                  | 0x8-0x10
 --------------------------------
| ....                           | 0x10-0x98
 --------------------------------
| Free List - 18                 | 0x98-0xA0
 --------------------------------
| Padding                        | 0xA0-0x1000
 --------------------------------
| Split Bitmap                   | 0x1000-0x9000
 ---------------------------------
| Free Bitmap                    | 0x9000-0x19000
 --------------------------------
| Allocable Block                | 0x40000000 bytes
+--------------------------------+
*/

struct buddy_block_t {
	void *block_base;

	struct buddy_free_list_t free_lists[MAX_ORDER];

	/* Padding to make bitmaps page aligned */
	uint8_t _padding_1[0xF60];

	uint64_t split_bitmap[SPLIT_BITMAP_SIZE];
	uint64_t free_bitmap[FREE_BITMAP_SIZE];
};

static inline void free_list_remove(struct buddy_free_list_t *list,
		struct buddy_chunk_t *chunk)
{
	if (list->head == chunk) {
		list->head = chunk->next;
		return;
	}

	struct buddy_chunk_t *prev = list->head;
	struct buddy_chunk_t *curr = prev->next;

	while (curr) {
		if (curr == chunk) {
			prev->next = curr->next;
			return;
		}
		prev = curr;
		curr = curr->next;
	}
}

static inline void free_list_push(struct buddy_free_list_t *list,
		struct buddy_chunk_t *chunk)
{
	chunk->next = list->head;
	list->head = chunk;
}

static inline struct buddy_chunk_t *free_list_pop(struct buddy_free_list_t
		*list)
{
	struct buddy_chunk_t *chunk = list->head;
	list->head = chunk->next;

	return chunk;
}

static inline void set_bit(uint64_t *map, uint64_t i)
{
	map[i >> 6] |= (1ULL << (i & 63));
}

static inline void clear_bit(uint64_t *map, uint64_t i)
{
	map[i >> 6] &= ~(1ULL << (i & 63));
}

static inline bool test_bit(uint64_t *map, uint64_t i)
{
	return (map[i >> 6] >> (i & 63)) & 1ULL;
}

extern long sys_mmap(uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags,
		uint64_t fd, uint64_t off);

extern long sys_munmap(uint64_t addr, uint64_t len);

void *blk_mmap(void)
{
	struct buddy_block_t *blk = (struct buddy_block_t *) sys_mmap(0,
			BLK_HEADER_SIZE + ALLOCABLE_BLK_SIZE,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
			-1, 0);

	if (blk == MAP_FAILED)
		return NULL;

	blk->block_base = (void *) ((char *) blk + BLK_HEADER_SIZE);

	blk->free_lists[MAX_ORDER - 1].head = blk->block_base;

	return blk;
}

void blk_munmap(struct buddy_block_t *blk)
{
	sys_munmap((uint64_t) blk, BLK_HEADER_SIZE + ALLOCABLE_BLK_SIZE);
}

#ifdef VERBOSE_DEBUG

#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)

#define ALLOC_REQ(ord) \
	fprintf(stderr, "[buddy] alloc: requested block of" \
			" order=%" PRIu64 "\n", (ord))

#define ALLOC_FAIL(ord) \
	fprintf(stderr, "[buddy] alloc: failed to satisfy request (order" \
			"=%" PRIu64 ")\n", (ord))

#define CHUNK_ALLOC(chunk) \
	fprintf(stderr, "[buddy] alloc: allocated block at=%p\n", (chunk))

#define CHUNK_FREE(ord, chunk) \
	fprintf(stderr, "[buddy] free: returned block at=%p to order=%" \
			PRIu64 "\n", (chunk), (ord))

#define SPLIT_CHUNK(ord, parent, buddy) \
	fprintf(stderr, "[buddy] alloc: split order=%" PRIu64 " block at=%p;" \
			" buddy block at=%p\n", (ord), (parent), (buddy))

#define SPLITMAP_SET(index) \
	fprintf(stderr, "[buddy] split bitmap: set bit at index=%" PRIu64 \
			"\n", (index))

#define SPLITMAP_CLR(index) \
	fprintf(stderr, "[buddy] split bitmap: cleared bit at index=%" PRIu64 \
			"\n", (index))

#define FREEMAP_SET(index) \
	fprintf(stderr, "[buddy] free bitmap: set bit at index=%" PRIu64 \
			"\n", (index))

#define FREEMAP_CLR(index) \
	fprintf(stderr, "[buddy] free bitmap: cleared bit at index=%" PRIu64 \
			"\n", (index))

#define CHUNK_MERGE(ord, child, buddy) \
	fprintf(stderr, "[buddy] merge: combined blocks %p and %p into" \
			" order=%" PRIu64 " block\n", (child), (buddy), (ord))

#define FREE_REQ(ord, chunk) \
	fprintf(stderr, "[buddy] free: requested release of order=%" PRIu64 \
			" block at=%p\n", (ord), (chunk))

#define FREELIST_POP(ord, chunk) \
	fprintf(stderr, "[buddy] freelist: poped chunk=%p of order=%" PRIu64 \
			"\n", (chunk), (ord))

#define FREELIST_PUSH(ord, chunk) \
	fprintf(stderr, "[buddy] freelist: pushed chunk=%p of order=%" PRIu64 \
			"\n", (chunk), (ord))

#define FREELIST_REM(ord, chunk) \
	fprintf(stderr, "[buddy] freelist: removed chunk=%p of order=%" \
			PRIu64 "\n", (chunk), (ord))

#else

#define PDEBUG(fmt, args...)

#define ALLOC_REQ(ord)

#define ALLOC_FAIL(ord)

#define CHUNK_ALLOC(chunk)

#define CHUNK_FREE(ord, chunk)

#define SPLIT_CHUNK(ord, parent, buddy)

#define SPLITMAP_SET(index)

#define SPLITMAP_CLR(index)

#define FREEMAP_SET(index)

#define FREEMAP_CLR(index)

#define CHUNK_MERGE(ord, child, buddy)

#define FREE_REQ(ord, chunk)

#define FREELIST_POP(ord, chunk)

#define FREELIST_PUSH(ord, chunk)

#define FREELIST_REM(ord, chunk)

#endif /* VERBOSE_DEBUG */

#ifdef DEBUG

#define LOG_BUF_SIZE 0x1000

static uint64_t alloc_id = 0;
static uint16_t idx = 0;

enum {
	LOG_ERR,
	LOG_ALLOC_REQ,
	LOG_ALLOC_OK,
	LOG_ALLOC_FAIL,
	LOG_FREE,
	LOG_SPLIT,
	LOG_MERGE
};

struct log_info_t {
	uint64_t seq;
	uint8_t type;
	uint8_t order;
	uint8_t chunk_order;
	uint32_t a;
	uint32_t b;
	uint32_t c;
};

static struct log_info_t events[LOG_BUF_SIZE];

static FILE *log = NULL;

static inline void flush_log()
{
	if (!log) {
		log = fopen("alloc.log", "wb");
	}
	fwrite(events, sizeof(struct log_info_t), idx, log);
	idx = 0;
}

static inline void log_event(struct log_info_t e)
{
	events[idx++] = e;
	if (idx == LOG_BUF_SIZE) {
		flush_log();
	}
}

__attribute__((destructor))
static void dump_log(void)
{
	flush_log();
	fclose(log);
}

#define LOG_EVENT(type_, order_, chunk_order_, a_, b_, c_) \
	log_event((struct log_info_t) { \
			.seq = alloc_id++, \
			.type = (type_), \
			.order = (order_), \
			.chunk_order = (chunk_order_), \
			.a = (a_), \
			.b = (b_), \
			.c = (c_) })

#define FLUSH_LOG() flush_log()

#else

#define LOG_EVENT(type_, order_, src_order_, a_, b_, c_)

#define FLUSH_LOG()

#endif /* DEBUG */

/* Precalculate (1 << (max_order - order)) - 1 */
static uint64_t map_base[MAX_ORDER] = {
	262143, 131071, 65535, 32767, 16383, 8191, 4095, 2047, 1023, 511, 255,
	127, 63, 31, 15, 7, 3, 1, 0
};

void *buddy_alloc(struct buddy_block_t *blk, uint64_t order)
{
	LOG_EVENT(LOG_ALLOC_REQ, order, 0, 0, 0, 0);
	ALLOC_REQ(order);

	if (blk->free_lists[order].head) {

		/* Get free chunk */
		struct buddy_chunk_t *chunk = free_list_pop(&blk->free_lists[order]);

		FREELIST_POP(order, chunk);
		CHUNK_ALLOC(chunk);

		uint64_t offset = (uint64_t) chunk - (uint64_t) blk->block_base;

		LOG_EVENT(LOG_ALLOC_OK, order, order, offset, order, 0);

		/* Calculate free bitmap index */
		offset = map_base[order] + (offset >> (order + 12));

		FREEMAP_SET(offset);

		/* Mark chunk as allocated and return */
		set_bit(blk->free_bitmap, offset);

		return chunk;
	}

	/* Find free chunk big enough to satisfy request */
	for (uint64_t i = order; i < MAX_ORDER; i++) {
		struct buddy_chunk_t *chunk = blk->free_lists[i].head;

		if (chunk) {
			chunk = free_list_pop(&blk->free_lists[i]);

			FREELIST_POP(i, chunk);

			uint64_t base = (uint64_t) blk->block_base;
			uint64_t chunk_offset = (uint64_t) chunk - base;
			uint64_t node = 0;
			uint64_t curr_order = i;
			uint64_t size = 4096ULL << curr_order;

			while (curr_order > order) {
				curr_order--;
				size >>= 1;

				uint64_t buddy_offset = chunk_offset ^ size;
				node = map_base[curr_order] + (chunk_offset >> (curr_order + 12));

				LOG_EVENT(LOG_SPLIT, order, curr_order + 1, chunk_offset, chunk_offset, buddy_offset);
				SPLIT_CHUNK(curr_order + 1, chunk, base + buddy_offset);
				FREELIST_PUSH(curr_order, base + buddy_offset);

				free_list_push(&blk->free_lists[curr_order], (struct buddy_chunk_t *) (base + buddy_offset));

				SPLITMAP_SET((node - 1) >> 1);

				set_bit(blk->split_bitmap, (node - 1) >> 1);
			}

			LOG_EVENT(LOG_ALLOC_OK, order, i, chunk_offset, curr_order, 0);
			FREEMAP_SET(node);
			CHUNK_ALLOC(chunk);

			set_bit(blk->free_bitmap, node);

			return chunk;
		}
	}

	LOG_EVENT(LOG_ALLOC_FAIL, order, 0, 0, 0, 0);
	ALLOC_FAIL(order);

	return NULL;
}

void buddy_free(struct buddy_block_t *blk, void *addr, uint64_t order)
{
	uint64_t base = (uint64_t) blk->block_base;
	uint64_t chunk_offset = (uint64_t) addr - base;
	uint64_t buddy_offset = chunk_offset ^ (4096 << order);
	uint64_t chunk_node = map_base[order] + (chunk_offset >> (order + 12));
	uint64_t buddy_node = map_base[order] + (buddy_offset >> (order + 12));

	LOG_EVENT(LOG_FREE, order, 0, chunk_offset, 0, 0);
	FREE_REQ(order, addr);
	FREEMAP_CLR(chunk_node);

	clear_bit(blk->free_bitmap, chunk_node);

	/* If order is 0 and buddy is free */
	if (!order && !test_bit(blk->free_bitmap, buddy_node)) {
		LOG_EVENT(LOG_MERGE, order, (order + 1), chunk_offset, buddy_offset, (chunk_offset < buddy_offset) ? chunk_offset : buddy_offset);
		CHUNK_MERGE(order + 1, base + chunk_offset, base + buddy_offset);
		FREELIST_REM(order, base + buddy_offset);

		/* Remove buddy from free list */
		free_list_remove(&blk->free_lists[order], (struct buddy_chunk_t *) (base + buddy_offset));

		/* Increment up to merged block order */
		order++;

		chunk_node = (chunk_node - 1) >> 1;
		/* Make sure new chunk offset is the base of merged chunk */
		chunk_offset = (chunk_offset < buddy_offset) ? chunk_offset : buddy_offset;
		buddy_offset = chunk_offset ^ (4096 << order);
		buddy_node = map_base[order] + (buddy_offset >> (order + 12));

		SPLITMAP_CLR(chunk_node);

		/* Clear split bit for merged chunk */
		clear_bit(blk->split_bitmap, chunk_node);
	}

	while (order < 18) {
		if (test_bit(blk->free_bitmap, buddy_node) || test_bit(blk->split_bitmap, buddy_node)) {
			break;
		}

		LOG_EVENT(LOG_MERGE, order, (order + 1), chunk_offset, buddy_offset, (chunk_offset < buddy_offset) ? chunk_offset : buddy_offset);
		CHUNK_MERGE(order + 1, base + chunk_offset, base + buddy_offset);
		FREELIST_REM(order, base + buddy_offset);

		free_list_remove(&blk->free_lists[order], (struct buddy_chunk_t *) (base + buddy_offset));

		order++;

		chunk_node = (chunk_node - 1) >> 1;
		chunk_offset = (chunk_offset < buddy_offset) ? chunk_offset : buddy_offset;
		buddy_offset = chunk_offset ^ (4096 << order);
		buddy_node = map_base[order] + (buddy_offset >> (order + 12));

		SPLITMAP_CLR(chunk_node);

		clear_bit(blk->split_bitmap, chunk_node);
	}

	CHUNK_FREE(order, base + chunk_offset);
	FREELIST_PUSH(order, base + chunk_offset);

	free_list_push(&blk->free_lists[order], (struct buddy_chunk_t *) (base + chunk_offset));
}
