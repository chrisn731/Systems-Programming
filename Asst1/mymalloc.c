#include <stdlib.h>
#include <stdio.h>

#include "mymalloc.h"

#define FATAL(x) die(x, filename, line_number)
#define HEAP_SIZE 4096

/*
 * Current thought process on what should be going down.
 *
 * ::mymalloc::
 * 	-> If user passes in 0 just return NULL
 * 	-> Now the main process of finding a block starts.
 * 	-> The heap always starts zero'd, so start at the top looking at header
 * 		data to see if the current data below there is in use.
 * 		-> If a block found is in use, increment the pointer and continue.
 * 		-> If an empty block is found prepare the block.
 * 	-> Once block is found we must:
 * 		1. Make sure we can actually store the size they ask for.
 * 		2. If we do have enough room:
 * 			set in use to 1, set block size, return ptr.
 * 	-> More steps to be added.
 *
 * ::myfree::
 * 	-> Need to add steps.
 */

/* Struct will look something like this, subject to change */
struct header_data {
	unsigned short block_size: 15;
	unsigned short free: 1;
};

static char myblock[HEAP_SIZE] = {0};

static void die(const char *err, const char *fname, const int line_num)
{
	fprintf(stderr, "::[File: %s : Line: %d] Error: %s\n", fname, line_num, err);
	exit(1);
}

static void initialize_heap(void)
{
	struct header_data *meta = (struct header_data *) myblock;

	meta->block_size = HEAP_SIZE - sizeof(*meta);
	meta->free = 1;
}

void *mymalloc(size_t size, const char *filename, const int line_number)
{
	struct header_data *meta, *next_block;
	char *heap_byte = myblock;
	char *const heap_boundary = myblock + HEAP_SIZE;

	if (size == 0)
		return NULL;

	/* The first 2 bytes of the heap should NEVER be 0 bytes */
	if (!myblock[0] && !myblock[1])
		initialize_heap();

	while (heap_byte < heap_boundary) {
		meta = (struct header_data *) heap_byte;
		if (meta->free && ((meta->block_size + sizeof(*meta)) >= size))
			break;
		heap_byte += (sizeof(*meta) + meta->block_size);

	}

	if (heap_byte >= heap_boundary)
		FATAL("Heap out of memory.");

	next_block = (struct header_data *) (heap_byte + sizeof(*meta) + size);
	next_block->free = 1;
	next_block->block_size = meta->block_size - size;
	meta->free = 0;
	meta->block_size = size;
	return (void *) heap_byte + sizeof(*meta);
}


static int not_in_range(void *ptr)
{
	char *ptr_to_free = (char *) ptr;

	return ((ptr_to_free < myblock) ||
		(ptr_to_free >= (myblock + HEAP_SIZE)));
}

void myfree(void *ptr, const char *filename, const int line_number)
{
	char *block_ptr = (char *) ptr;

	if (!block_ptr)
		FATAL("Attempting to free NULL pointer.");
	if (not_in_range(ptr))
		FATAL("Attempting to free pointer not in range.");
}
