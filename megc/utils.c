/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include "meg.h"

struct mem {
	struct mem *next;
	char data[];
};

/* List of memory blocks. */
LOCAL static struct mem *mlist;
/* Allocation count. */
LOCAL static int allc;
/* Bytes allocated. */
LOCAL static size bytes;

void meminit() {
	/* Nothing for now. */
}

void memdnit() {
	if (fdump) {
		info(
			"--Memory Information--\n"
			"  |Blocks allocated: %d\n"
			"  |Bytes allocated: %zu",
			allc,
			bytes
		);
	}

	while (mlist) {
		struct mem *next = mlist->next;
		free(mlist);

		allc--;
		mlist = next;
	}

	if (allc != 0)
		adeus("Allocator error.");
}

void *alloc(size n) {
	struct mem *mem = malloc(n + 8);
	mem->next = mlist;
	mlist = mem;

	bytes += n;
	allc++;
	return mem->data;
}
