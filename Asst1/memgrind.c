#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "mymalloc.h"


int main(void)
{
	/* This is just here to show how we will be computing time taken */
	clock_t start, end;
	double total_time;

	/* Start the clock */
	start = clock();

	/* Beep Boop ... Code goes here */

	/* End the clock */
	end = clock();

	/* Calculate time taken */
	total_time = (double)(end - start) / CLOCKS_PER_SEC;

	return 0;
}
