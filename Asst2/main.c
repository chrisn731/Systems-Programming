#include <stdlib.h>
#include <stdio.h>

#include "dirhandler.h"
#include "data.h"

#define UNUSED(x) ((void) x)
/*
 * Note for Nelli:
 * 	Just needed a main to invoke and test my functions to
 * 	make sure everything is in the right place.
 * 	Also, make sure that even if argv[1] does have an ending '/'
 * 	that you malloc it to a new pointer just to make freeing memory easier
 * 	at the end. ty
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

int main(int argc, char **argv)
{
	struct thread_data *t_data;
	struct file_database *db;
	UNUSED(argc);

	db = new_db();
	t_data = new_thread_data(db, argv[1]);
	start_dirhandler(t_data);
	/* At this point, all newly spawned threads have finished. */
	print_db(db);
	free_database(db);
	return 0;
}
