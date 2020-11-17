#include <stdlib.h>
#include <stdio.h>

#include "dirhandler.h"
#include "data.h"

#define UNUSED(x) ((void) x)
/*
 * Note for Nelli:
 * 	Just needed a main to invoke and test my functions
 */
int main(int argc, char **argv)
{
	struct thread_data t_data;
	t_data.filepath = argv[1];
	UNUSED(argc);
	start_dirhandler(&t_data);
	return 0;
}
