#include <stdlib.h>
#include <stdio.h>

#include "dirhandler.h"
#include "data.h"

#define UNUSED(x) ((void) x)
/*
 * Note for Nelli:
 * 	Just needed a main to invoke and test my functions to
 * 	make sure everything is in the right place.
 */

static void print_db(struct file_database *db)
{
	struct file_node *file_ptr = db->first_node;
	struct file_word *word_ptr;
	printf("DB:\n");

	while (file_ptr != NULL) {
		printf("\t%s:\n", file_ptr->filepath);
		word_ptr = file_ptr->word;
		while (word_ptr != NULL) {
			printf("\t\t%s\n", word_ptr->word);
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
	print_db(db);
	free_database(db);
	return 0;
}
