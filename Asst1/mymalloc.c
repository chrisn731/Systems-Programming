#include <stdlib.h>
#include <stdio.h>

#include "mymalloc.h"

#define WARN(x) warn(x, filename, line_number)

/* Only change this value to configure heap size */
#define HEAP_SIZE 4096

/* Data structure used to access our values stored in our header data. */
struct header_data {
	unsigned short block_size: 15;
	unsigned short free: 1;
};

static char myblock[HEAP_SIZE] = {0};

/*
 * Purpose: Print warning message to user that something that has gone wrong, but
 * is not fatal to the running process.
 * Return Value: None.
 */
static void warn(const char *warning, const char *fname, int line_num)
{
	fprintf(stderr, "::[File: %s: Line %d] WARNING: %s\n", fname, line_num, warning);
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
void *mymalloc(size_t size, const char *filename, int line_number)
{
	struct header_data *meta;
	char *heap_byte = myblock;
	const char *const heap_boundary = myblock + HEAP_SIZE;

	if (!size)
		return NULL;

	/* If the first 2 bytes of our heap are 0 bytes, then we haven't initialized */
	if (!myblock[0] && !myblock[1])
		initialize_heap();

	/* Go through the heap to find an empty block that can fit the requested size. */
	do {
		meta = (struct header_data *) heap_byte;
		if (meta->free && (meta->block_size >= size))
			break;
		heap_byte += sizeof(*meta) + meta->block_size;
	} while (heap_byte < heap_boundary);

	/* Heap byte now becomes the pointer to mutable memory we return to the user. */
	heap_byte += sizeof(*meta);

	/* Ensure that the byte that we return to the user is not out of bounds */
	if (heap_byte >= heap_boundary) {
		WARN("Heap out of memory.");
		return NULL;
	}

	/*
	 * If our block is bigger than our requested size we need to split the block.
	 * If we are splitting, make sure that we have enough bytes the end to
	 * actually split. For example: Given a 4096 byte heap, if our meta
	 * data is 2 bytes and our requested size is 4093, that means
	 * we have 1 byte left to work with.
	 * We cannot edit that last byte without going out of range.
	 */
	if (meta->block_size > size &&
	((heap_byte + size + sizeof(*meta) / 2) < heap_boundary)) {
		struct header_data *next_block = (struct header_data *) (heap_byte + size);
		next_block->free = 1;
		next_block->block_size = meta->block_size - (size + sizeof(*next_block));
	}
	meta->free = 0;
	meta->block_size = size;
	return (void *) heap_byte;
}

/*
 * Purpose: Takes in a pointer and tests to see if it is in range of our heap.
 * Return Value: 0 if in range, non-zero otherwise.
 */
static int not_in_range(void *ptr)
{
	char *ptr_to_free = ptr;
	const char *const heap_boundary = myblock + HEAP_SIZE;

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
	unsigned int free_block_size;
	const char *const heap_boundary = myblock + HEAP_SIZE;
	char *heap_byte = myblock;

	do {
		first_meta = (struct header_data *) heap_byte;
		heap_byte += first_meta->block_size + sizeof(*first_meta);
		if (first_meta->free) {
			while ((heap_byte + sizeof(*first_meta) / 2) < heap_boundary) {
				next_meta = (struct header_data *) heap_byte;
				if (!next_meta->free)
					break;
				free_block_size = next_meta->block_size + sizeof(*next_meta);
				first_meta->block_size += free_block_size;
				heap_byte += free_block_size;
			}
		}
	} while ((heap_byte + sizeof(*first_meta) / 2) < heap_boundary);
}

/*
 * Purpose: Frees an allocated section of memory from our heap to be used
 * later.
 * Return Value: None.
 */
void myfree(void *ptr, const char *filename, int line_number)
{
	struct header_data *meta;
	const char *err = NULL;

	/* Check main 3 error cases: NULL, not in range, and non malloc'd */
	if (!ptr)
		err = "Attempting to free NULL pointer.";
	else if (not_in_range(ptr))
		err = "Attempting to free pointer not in range.";
	else if (non_mymalloc_ptr(ptr))
		err = "Attempting to free nonmalloc'd pointer.";
	if (err) {
		WARN(err);
		return;
	}

	/* Get the meta data of the valid block the user passed in */
	meta = (struct header_data *) ((char *) ptr - sizeof(*meta));

	/* check if the block has been freed already */
	if (meta->free) {
		WARN("Attempting to redudantly free pointer.");
		return;
	}
	meta->free = 1;

	/* Go through the heap and combine free'd blocks */
	coalesce_blocks();
}
