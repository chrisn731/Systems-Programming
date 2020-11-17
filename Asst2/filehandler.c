#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <err.h>

#include "filehandler.h"
#include "data.h"



void *start_filehandler(void *data)
{
	printf("%s\n", (char*) data);
	return NULL;
}
