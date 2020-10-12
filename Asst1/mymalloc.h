#ifndef _MY_MALLOC_H
#define _MY_MALLOC_H

#include <stdlib.h> /* Only here so the compiler knows what size_t is */

#define malloc(x) mymalloc(x, __FILE__, __LINE__)
#define free(x) myfree(x, __FILE__, __LINE__)

void *mymalloc(size_t size, const char *filename, const int line_number);
void myfree(void *ptr, const char *filename, const int line_number);

#endif /* _MY_MALLOC_H */
