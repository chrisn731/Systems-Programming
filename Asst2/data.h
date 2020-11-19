#ifndef _DATA_H
#define _DATA_H

#include <pthread.h> /* pthread_mutex_t */

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

/* Still need to figure out the details of below. */
struct file_word *new_word(char *);
struct file_node *new_file(char *);
struct file_database *new_db(void);
struct thread_data *new_thread_data(struct file_database *, char *);
void free_word(struct file_word *);
void free_database(struct file_database *);
struct file_word *search_for_fileword(const struct file_node *, const char *);
void insert_fileword(struct file_node *file, struct file_word *);

#endif /* _DATA_H */
