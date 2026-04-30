#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "buddy_alloc.h"

#define ITER 10000000
#define NUM_ALLOCATIONS 262144
#define MAX_ORDER 19

struct alloc_info_t {
	void *addr;
	uint8_t order;
};

void timespec_add(struct timespec *a, const struct timespec *b) {
	a->tv_sec  += b->tv_sec;
	a->tv_nsec += b->tv_nsec;

	// Normalize nanoseconds
	if (a->tv_nsec >= 1000000000L) {
		a->tv_sec += 1;
		a->tv_nsec -= 1000000000L;
	}
}

int main(void) {
	void *blk = blk_mmap();

	struct alloc_info_t alloc_info[NUM_ALLOCATIONS] = {0};
	struct timespec alloc_start, alloc_end;
	struct timespec alloc_start_total, alloc_end_total;
	struct timespec free_start, free_end;
	struct timespec free_start_total, free_end_total;
	int allocs = 0, frees = 0, total_fails = 0, i, j;
	int null_allocs[MAX_ORDER] = {0};

	srand(time(NULL));

	memset(&alloc_start_total, 0, sizeof(struct timespec));
	memset(&alloc_end_total, 0, sizeof(struct timespec));
	memset(&free_start_total, 0, sizeof(struct timespec));
	memset(&free_end_total, 0, sizeof(struct timespec));
	memset(alloc_info, 0, sizeof(alloc_info));

	for (i = 0, j = 0; i < ITER; i++) {
		int do_alloc = rand() % 2;

		if (do_alloc) {
			if (j < NUM_ALLOCATIONS) {
				alloc_info[j].order = rand() % MAX_ORDER;
				clock_gettime(CLOCK_MONOTONIC_RAW, &alloc_start);
				alloc_info[j].addr = buddy_alloc(blk, alloc_info[j].order);
				clock_gettime(CLOCK_MONOTONIC_RAW, &alloc_end);

				if (!alloc_info[j].addr) {
					total_fails++;
					null_allocs[alloc_info[j].order]++;
					alloc_info[j].order = -1;

					for (int k = 0; k < j; k++) {
						clock_gettime(CLOCK_MONOTONIC_RAW, &free_start);
						buddy_free(blk, alloc_info[k].addr, alloc_info[k].order);
						clock_gettime(CLOCK_MONOTONIC_RAW, &free_end);

						timespec_add(&free_start_total, &free_start);
						timespec_add(&free_end_total, &free_end);

						alloc_info[k].addr = NULL;
						alloc_info[k].order = -1;
						frees++;
					}

					j = 0;
					continue;
				}

				timespec_add(&alloc_start_total, &alloc_start);
				timespec_add(&alloc_end_total, &alloc_end);

				j++;
				allocs++;
			}
		} else {
			if (j > 0) {
				int idx = rand() % j;
				clock_gettime(CLOCK_MONOTONIC_RAW, &free_start);
				buddy_free(blk, alloc_info[idx].addr, alloc_info[idx].order);
				clock_gettime(CLOCK_MONOTONIC_RAW, &free_end);

				timespec_add(&free_start_total, &free_start);
				timespec_add(&free_end_total, &free_end);

				alloc_info[idx].addr = alloc_info[j - 1].addr;
				alloc_info[idx].order = alloc_info[j - 1].order;
				alloc_info[j - 1].addr = NULL;
				alloc_info[j - 1].order = -1;

				j--;
				frees++;
			} else {
				alloc_info[j].order = rand() % MAX_ORDER;
				clock_gettime(CLOCK_MONOTONIC_RAW, &alloc_start);
				alloc_info[j].addr = buddy_alloc(blk, alloc_info[j].order);
				clock_gettime(CLOCK_MONOTONIC_RAW, &alloc_end);

				if (!alloc_info[j].addr) {
					null_allocs[alloc_info[j].order]++;
					alloc_info[j].order = -1;
					total_fails++;
					continue;
				}

				j++;
				allocs++;
			}
		}
	}

	blk_munmap(blk);

	printf("With max %d objects allocated at once over %d iterations\n", NUM_ALLOCATIONS, ITER);

	long alloc_sec = alloc_end_total.tv_sec - alloc_start_total.tv_sec;
	long alloc_nsec = alloc_end_total.tv_nsec - alloc_start_total.tv_nsec;

	if (alloc_nsec < 0) {
		alloc_nsec += 1000000000;
		alloc_sec -= 1;
	}

	long double alloc_elapsed = (alloc_sec + (long double) alloc_nsec / 1000000000.0) * 1e3;

	printf("%d allocations took: %Lfms\n", allocs, alloc_elapsed);

	long free_sec = free_end_total.tv_sec - free_start_total.tv_sec;
	long free_nsec = free_end_total.tv_nsec - free_start_total.tv_nsec;

	if (free_nsec < 0) {
		free_nsec += 1000000000;
		free_sec -= 1;
	}

	long double free_elapsed = (free_sec + (long double) free_nsec / 1000000000.0) * 1e3;

	printf("%d frees took: %Lfms\n", frees, free_elapsed);

	printf("Allocation fails by order\n");
	for (int i = 0; i < MAX_ORDER; i++) {
		printf("%d:	%d (%.2f%% of all fails)\n", i, null_allocs[i], (null_allocs[i] / (double) total_fails) * 100.0);
	}
	printf("Total fails:	%d\n", total_fails);

	return 0;
}
