#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "data.h"

/*
 * Purspose: Allocate space for a new word.
 * Return Value: Pointer to file word type.
 */
struct file_word *new_word(char *word)
{
	struct file_word *new_word;

	if (!(new_word = malloc(sizeof(*new_word))))
		err(-1, "Memory alloc error.");
	new_word->word = word;
	new_word->count = 1;
	new_word->left = new_word->right = NULL;
	new_word->prob = 0;
	return new_word;
}

/*
 * Purpose: Free file word type from memory.
 * Return Value: None.
 */
static void free_words(struct file_word *word)
{
	if (word) {
		free_words(word->left);
		free_words(word->right);
		free(word->word);
		free(word);
	}
}

/*
 * Purpose: Allocate space for a new file entry.
 * Return Value: Pointer to file type.
 */
struct file_node *new_file(char *pathname)
{
	struct file_node *new_file;

	if (!(new_file = malloc(sizeof(*new_file))))
		err(-1, "Memory alloc error.");
	new_file->word = NULL;
	new_file->next = NULL;
	new_file->num_words = 0;
	new_file->filepath = pathname;
	return new_file;
}

/*
 * Purpose: Free a file from memory.
 * Return Value: None.
 * NOTE: There should NEVER be a case where a thread should be freeing
 * an individual file. Too dangerous. Only called ever by free_database().
 */
static void free_file(struct file_node *file)
{
	free_words(file->word);
	free(file->filepath);
	free(file);
}

/*
 * Purpose: Allocate space for a new file database, which includes a
 * pointer to it's first file node, and a pthread mutex. This mutex
 * is the one that all threads will use to lock to access the file
 * node list.
 * Return Value: Pointer to file database.
 * NOTE: This function automatically init's the mutex. No need to try
 * and mutate it afterwards.
 */
struct file_database *new_db(void)
{
	struct file_database *new_db;

	if (!(new_db = malloc(sizeof(*new_db))))
		err(-1, "Memory alloc error.");
	new_db->first_node = NULL;
	pthread_mutex_init(&new_db->mut, NULL);
	return new_db;
}

/*
 * Purpose: Free the file database from memory. This includes all it's
 * stored files, and all their stored words.
 * Return Value: None.
 * NOTE: This is to only be used AFTER no more data is needed from the
 * the database.
 */
void free_database(struct file_database *db)
{
	struct file_node *file_parser = db->first_node;
	struct file_node *temp;

	while (file_parser != NULL) {
		temp = file_parser->next;
		free_file(file_parser);
		file_parser = temp;
	}
	free(db);
}

/*
 * Purpose: Allocate space for a struct that we can pass to our threads
 * so they have the necessary data to do their jobs. All they need is
 * a pointer to the file database, and the filepath of the file they will
 * act upon.
 * Return Value: Pointer to Thread Data.
 */
struct thread_data *new_thread_data(struct file_database *db, char *fp)
{
	struct thread_data *new;

	if (!(new = malloc(sizeof(*new))))
		err(-1, "Memory alloc error.");
	new->db_ptr = db;
	new->filepath = fp;
	return new;
}
