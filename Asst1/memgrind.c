#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "mymalloc.h"

#define SIZE_A 120
#define SIZE_B 120
#define SIZE_C 240
#define SIZE_D 64
#define SIZE_E 120

/* Don't change these */
#define NUM_LARGE_CHUNKS 32
#define NUM_SMALL_CHUNKS 96

/*
 * Purpose: Malloc 1 byte and immediately free it 120 times
 * Return value: None.
 */
static void workload_A(void)
{
	int i;

	for (i = 0; i < SIZE_A; i++) {
		char *a = malloc(sizeof(char));
		free(a);
	}
}

/*
 * Purpose: Malloc 1 byte, store it in a pointer array, and free them all after looping 120 times
 * Return value: None.
 */
static void workload_B(void)
{
	char *arr[SIZE_B];
	int i;

	for (i = 0; i < SIZE_B; i++)
		arr[i] = malloc(sizeof(char));

	for (i = 0; i < SIZE_B; i++)
		free(arr[i]);
}

static void workload_C(void)
{
	char *arr[SIZE_C/2];
	int i, num, use_malloc;
	int pos = -1, num_mallocs = 0;

	/* Create a random num in the range of 1-100. Malloc if num < 50; free otherwise
	 * HOWEVER: if there's nothing to free, use malloc. If we can't malloc anymore, use free */
	for (i = 0; i < SIZE_C; i++) {
		num = (rand() % 100) + 1;
		if (num < 50)
			use_malloc = (num_mallocs < (SIZE_C/2)) ? 1 : 0;
		else
			use_malloc = (pos >= 0) ? 0 : 1;

		if (use_malloc) {
			pos++;
			arr[pos] = malloc(sizeof(char));
			num_mallocs++;
		} else {
			free(arr[pos]);
			pos--;
		}
	}
}

/*
 * Purpose: Test filling the entire heap with larger blocks and then freeing them one
 * at a time, replacing that block with smaller ones. How does the running time compare
 * to workload B?
 * Total 128 mallocs - 32 big chunks and 96 smaller chunks
 * Return value: None.
 */
static void workload_D(void)
{
	char *large_chunks[NUM_LARGE_CHUNKS];
	char *small_chunks[NUM_SMALL_CHUNKS];
	int i, j, k = 0;
	int SM_L_ratio = NUM_SMALL_CHUNKS / NUM_LARGE_CHUNKS;

	/*
	 * NUM_LARGE_CHUNKS chunks each with size (4096 / NUM_LARGE_CHUNKS) - 2
	 * -2 accounts for 2 byte meta data per block, so we perfectly fill 4096 bytes.
	 */
	for (i = 0; i < NUM_LARGE_CHUNKS; i++)
		large_chunks[i] = malloc(((4096 / NUM_LARGE_CHUNKS) - 2) * sizeof(char));

	/*
	 * Free 1 big chunk at a time (latest allocated to first allocated),
	 * then fill it with smaller, ((4096 / NUM_SMALL_CHUNKS) -2) byte chunks.
	 * -2 accounts for meta data.
	 */
	for (i = NUM_LARGE_CHUNKS - 1; i >= 0; i--) {
		free(large_chunks[i]);
		for (j = 0; j < SM_L_ratio ; j++) {
			small_chunks[k] = malloc(((4096 / NUM_SMALL_CHUNKS) - 2) * sizeof(char));
			k++;
		}
	}

	/* At this point, the heap is filled with 42 byte x 128 chunks (nearly 4096) */
	for (i = 0; i < NUM_SMALL_CHUNKS; i++)
		free(small_chunks[i]);
}

/*
 * Purpose: ensures that free() combines adjacent free blocks and checks its impact
 * on runtime
 * Return value: None.
 */
static void workload_E(void)
{
	char *arr[SIZE_E];
	int i, j, k = 1;
	char *ptr;

	for (i = 0; i < SIZE_E; i++)
		arr[i] = malloc(sizeof(char));


	for (j = (SIZE_E/2) - 1; j >= 0; j--) {
		free(arr[j]);
		free(arr[j+k]);
		k += 2;
	}

	ptr = malloc(4094 * sizeof(char));
	free(ptr);
}

int main(void)
{
	clock_t start, end;
	double total_time;
	double data[50][5];
	void (*fptr[5])(void) = {workload_A, workload_B, workload_C, workload_D, workload_E};
	int i, j;

	/* Run each workload and store the runtime into a 2D array */
	srand(time(0));
	for (i = 0; i < 50; i++) {
		for (j = 0; j < 5; j++) {
			start = clock();
			fptr[j]();
			end = clock();
			data[i][j] = (double)(end - start) / CLOCKS_PER_SEC;
		}
	}

	for (j = 0; j < 5; j++) {
		total_time = 0;
		for (i = 0; i < 50; i++) {
			total_time += data[i][j];
		}
		printf("Workload_%c mean runtime: %f\n", ('A' + j), (total_time / 50));
	}

	return 0;
}
