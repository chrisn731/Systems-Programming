#include <stdlib.h>
#include <stdio.h>

#include "mymalloc.h"

#define FATAL(x) die(x, filename, line_number)
#define HEAP_SIZE 4096

/* Struct will look something like this, subject to change */
struct header_data {
	unsigned short block_size: 15;
	unsigned short free: 1;
};
#define BYTES_TO_NEXT_META(size) (sizeof(struct header_data) + (size))
#define HEADER_DATA(x) (struct header_data *)(x)

static char myblock[HEAP_SIZE] = {0};

/*
 * Purpose: Error exit function. Prints the error as well as the file and line number
 * of where the error occured.
 * Return Value: Does not return.
 */
static void die(const char *err, const char *fname, const int line_num)
{
	fprintf(stderr, "::[File: %s: Line: %d] Error: %s\n", fname, line_num, err);
	exit(1);
}

/*
 * Purpose: Initlialize the first 2 bytes of the heap to be meta data. This
 * allows future blocks to be built and split off from this first block.
 * Return Value: None.
 */
static void initialize_heap(void)
{
	struct header_data *meta = (struct header_data *) myblock;

	meta->block_size = HEAP_SIZE - sizeof(*meta);
	meta->free = 1;
}

/*
 * Purpose: Returns a pointer to a chunk of memory in our "heap".
 * Return Value: Pointer to the first byte of allocated memory.
 */
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

	/*
	 * go through the heap to find an empty block that can fit the block size
	 * and the meta data for the block that will be after.
	 */
	do {
		meta = (struct header_data *) heap_byte;
		if (meta->free && (meta->block_size >= (size + sizeof(*meta))))
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

/*
 * Purpose: Takes in a pointer and tests to see if it is in range of our heap.
 * Return Value: 0 if in range, non-zero otherwise.
 */
static int not_in_range(void *ptr)
{
	char *ptr_to_free = (char *) ptr;
	char *heap_boundary = myblock + HEAP_SIZE;

	return (ptr_to_free < myblock) || (ptr_to_free >= heap_boundary);
}

/*
 * Purpose: Takes in a pointer and tests to see if it is a pointer to an allocated
 * block of memory.
 * Return Value: 0 if it is a valid mymalloc'd pointer, non-zero otherwise.
 */
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

/*
 * Purpose: Goes through the heap and combines free blocks into one block. Gets
 * called after every call to myfree().
 * Return Value: None.
 */
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

/*
 * Purpose: Frees an allocated section of memory from our heap to be used
 * later.
 * Return Value: None.
 */
void myfree(void *ptr, const char *filename, const int line_number)
{
	struct header_data *meta;
	char *block_ptr = (char *) ptr;

	/* Check main 3 error cases */
	if (!block_ptr)
		FATAL("Attempting to free NULL pointer.");
	if (not_in_range(ptr))
		FATAL("Attempting to free pointer not in range.");
	if (non_mymalloc_ptr(ptr))
		FATAL("Attempting to free nonmalloc'd pointer");

	block_ptr -= sizeof(*meta);
	meta = (struct header_data *) block_ptr;
	/* check if the block has been freed already */
	if (meta->free)
		FATAL("Attempting to redudantly free pointer");
	else
		meta->free = 1;

	/* Go through the heap and combine free'd blocks */
	coalesce_blocks();
}
