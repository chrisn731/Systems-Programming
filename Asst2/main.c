#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dirhandler.h"
#include "data.h"

#define UNUSED(x) ((void) x)
/*
 * Note for Nelli:
 * 	Just needed a main to invoke and test my functions to
 * 	make sure everything is in the right place.
 * 	Also, make sure that even if argv[1] does have an ending '/'
 * 	that you malloc it to a new pointer just to make freeing memory easier
 * 	at the end.
 */

static void print_db(struct file_database *db)
{
	struct file_node *file_ptr = db->first_node;
	struct file_word *word_ptr;
	printf("DB:\n");

	while (file_ptr != NULL) {
		printf("\t%s (%d):\n", file_ptr->filepath, file_ptr->num_words);
		word_ptr = file_ptr->word;
		while (word_ptr != NULL) {
			printf("\t\t%s (%lf)\n", word_ptr->word, word_ptr->prob);
			word_ptr = word_ptr->next;
		}
		file_ptr = file_ptr->next;
	}
}

static char *parent_dir_alloc(const char *fpath)
{
	char *parent_dir;
	int fpath_length;

	fpath_length = strlen(fpath);
	parent_dir = malloc(sizeof(*parent_dir) * (fpath_length + 1));
	strcpy(parent_dir, fpath);
	parent_dir[fpath_length] = '\0';
	return parent_dir;
}

int main(int argc, char **argv)
{
	struct thread_data *t_data;
	struct file_database *db;
	char *parent_dir;
	UNUSED(argc);

	db = new_db();
	parent_dir = parent_dir_alloc(argv[1]);
	t_data = new_thread_data(db, parent_dir);
	start_dirhandler(t_data);
	/* At this point, all newly spawned threads have finished. */
	print_db(db);
	free_database(db);
	return 0;
}
