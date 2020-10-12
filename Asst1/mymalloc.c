#include <stdlib.h>
#include <stdio.h>

#include "mymalloc.h"

#define HEAP_SIZE 4096

static char myblock[HEAP_SIZE];

static void die(const char *err, const char *fname, const int line_num)
{
	fprintf(stderr, "::Error: %s\n::Error in: %s on line number: %d\n", err, fname, line_num);
	exit(1);
}

void *mymalloc(size_t size, const char *filename, const int line_number)
{

	return NULL;
}


void myfree(void *ptr, const char *filename, const int line_number)
{

}
