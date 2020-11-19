#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include "data.h"

struct file_word *new_word(char *word)
{
	struct file_word *new_word;

	if (!(new_word = malloc(sizeof(*new_word))))
		errx(-1, "Out of memory");
	new_word->word = word;
	new_word->count = 1;
	new_word->next = NULL;
	new_word->prob = 0;
	return new_word;
}

void free_word(struct file_word *word)
{
	free(word->word);
	free(word);
}

struct file_node *new_file(char *pathname)
{
	struct file_node *new_file;

	if (!(new_file = malloc(sizeof(*new_file))))
		errx(-1, "Out of memory");
	new_file->word = NULL;
	new_file->next = NULL;
	new_file->num_words = 0;
	new_file->filepath = pathname;
	return new_file;
}

/*
 * There should NEVER be a case where a thread should be freeing
 * an individual file. Only called ever by free_database().
 */
static void free_file(struct file_node *file)
{
	struct file_word *word = file->word;
	struct file_word *temp;

	while (word != NULL) {
		temp = word->next;
		free_word(word);
		word = temp;
	}
	free(file->filepath);
	free(file);
}

struct file_database *new_db(void)
{
	struct file_database *new_db;

	if (!(new_db = malloc(sizeof(*new_db))))
		errx(-1, "Out of memory");
	new_db->first_node = NULL;
	pthread_mutex_init(&new_db->mut, NULL);
	return new_db;
}

/*
 * WARNING: This will destory the ENITRE file database.
 * Freeing all data that it contains.
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

struct thread_data *new_thread_data(struct file_database *db, char *fp)
{
	struct thread_data *new;

	if (!(new = malloc(sizeof(*new))))
		errx(-1, "out of memory");
	new->db_ptr = db;
	new->filepath = fp;
	return new;
}

void insert_fileword(struct file_node *file, struct file_word *new_word)
{
	struct file_word **parser = &file->word;

	while (*parser != NULL)
		parser = &(*parser)->next;

	*parser = new_word;
	new_word->next = NULL;
}

struct file_word *
search_for_fileword(const struct file_node *file, const char *word)
{
	struct file_word *parser = file->word;

	while (parser != NULL) {
		if (!strcmp(parser->word, word))
			return parser;
		parser = parser->next;
	}
	return NULL;
}
