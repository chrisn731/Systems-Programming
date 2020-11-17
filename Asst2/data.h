#ifndef _DATA_H
#define _DATA_H
#include <pthread.h>


struct file_word {
	struct file_word *next;
	char *word;
	double prob;
	unsigned int count;
};

struct file_node {
	struct file_word *word;
	struct file_node *next;
	char *filepath;
	unsigned int num_words;
};

struct file_database {
	struct file_node *first_node;
	pthread_mutex_t mut;
};

struct thread_data {
	struct file_database *db_ptr;
	char *filepath;
};

#endif /* _DATA_H */
