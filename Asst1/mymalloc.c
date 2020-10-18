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
#define BYTES_TO_NEXT_META(size) (sizeof(struct header_data) + (size))

static char myblock[HEAP_SIZE] = {0};

static void die(const char *err, const char *fname, const int line_num)
{
	fprintf(stderr, "::[File: %s: Line: %d] Error: %s\n", fname, line_num, err);
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
	const char *const heap_boundary = myblock + HEAP_SIZE;

	if (size == 0)
		return NULL;

	/* The first 2 bytes of the heap should NEVER be 0 bytes */
	if (!myblock[0] && !myblock[1])
		initialize_heap();

	do {
		meta = (struct header_data *) heap_byte;
		if (meta->free && ((meta->block_size + sizeof(*meta)) > size))
			break;
		heap_byte += (sizeof(*meta) + meta->block_size);
	} while (heap_byte < heap_boundary);

	if (heap_byte >= heap_boundary)
		FATAL("Heap out of memory.");

	next_block = (struct header_data *) (heap_byte + sizeof(*meta) + size);
	next_block->free = 1;
	next_block->block_size = meta->block_size - size;
	meta->free = 0;
	meta->block_size = size;
	return (void *) (heap_byte + sizeof(*meta));
}


static int not_in_range(void *ptr)
{
	char *ptr_to_free = (char *) ptr;
	char *heap_boundary = myblock + HEAP_SIZE;

	return (ptr_to_free < myblock) || (ptr_to_free >= heap_boundary);
}

static int non_mymalloc_ptr(void *ptr)
{
	struct header_data *meta;
	const char *const heap_boundary = myblock + HEAP_SIZE;
	char *heap_byte = myblock;
	char *block_ptr;

	do {
		block_ptr = heap_byte + sizeof(*meta);
		if (ptr == block_ptr)
			return 0;
		meta = (struct header_data *) heap_byte;
		heap_byte += sizeof(*meta) + meta->block_size;
	} while (heap_byte < heap_boundary);

	return 1;
}

static void coalesce_blocks(void)
{
	struct header_data *first_meta, *next_meta;
	const char *const heap_boundary = myblock + HEAP_SIZE;
	char *heap_byte = myblock;

	do {
		first_meta = (struct header_data *) heap_byte;
		heap_byte += first_meta->block_size + sizeof(*first_meta);
		next_meta = (struct header_data *) heap_byte;
		if (first_meta->free) {
			while (next_meta->free) {
				first_meta->block_size += next_meta->block_size + sizeof(*next_meta);
				heap_byte += next_meta->block_size + sizeof(*next_meta);
				next_meta = (struct header_data *) heap_byte;
			}
		}
	} while (heap_byte < heap_boundary);
}

void myfree(void *ptr, const char *filename, const int line_number)
{
	struct header_data *meta;
	char *block_ptr = (char *) ptr;

	if (!block_ptr)
		FATAL("Attempting to free NULL pointer.");
	if (not_in_range(ptr))
		FATAL("Attempting to free pointer not in range.");
	if (non_mymalloc_ptr(ptr))
		FATAL("Attempting to free nonmalloc'd pointer");

	block_ptr -= sizeof(*meta);
	meta = (struct header_data *) block_ptr;
	if (meta->free)
		FATAL("Attempting to redudantly free pointer");
	else
		meta->free = 1;

	coalesce_blocks();
}
