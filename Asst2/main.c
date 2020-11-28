#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <err.h>

#include "dirhandler.h"
#include "data.h"

#define UNUSED(x) ((void) x)

struct word_info {
	struct word_info *next;
	double mean, file1_prob, file2_prob;
};

struct file_pair {
	struct file_pair *next;
	struct file_node *file1;
	struct file_node *file2;
	struct mean_word *mean_list;
	unsigned int num_words;
};

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
	if (fpath[fpath_length - 1] != '/')
		fpath_length++;

	parent_dir = malloc(sizeof(*parent_dir) * (fpath_length + 1));
	strcpy(parent_dir, fpath);
	parent_dir[fpath_length - 1] = '/';
	parent_dir[fpath_length] = '\0';
	return parent_dir;
}

/*
 * Purpose: Creates a list of means from the token distributions found
 * from both files in a file pair. Each node represents the occurrence of
 * a particular token averaged between the two files.
 * Return value: None.
 */
static void create_mean_list(struct file_pair **fp)
{
	struct file_word *file1_word_ptr = (*fp)->file1->word;
	struct file_word *file2_word_ptr = (*fp)->file2->word;
	struct word_info *first_word = NULL;
	struct word_info **fw = &first_word;

	while (file1_word_ptr != NULL) {
		if (!(strcmp(file1_word_ptr->word, file2_word_ptr->word))) {
			*fw = malloc(sizeof(struct word_info));
			(*fw)->file1_prob = file1_word_ptr->prob;
			(*fw)->file2_prob = file2_word_ptr->prob;
			(*fw)->mean = ((*fw)->file1_prob + (*fw)->file2_prob) / 2;

			file1_word_ptr = file1_word_ptr->next;
			file2_word_ptr = file2_word_ptr->next;
			fw = &(*fw)->next;
		} else {

		}
	}
}

/*
 * Purpose: Compares the words found every file pair in the database.
 * Return value: Pointer to the first file pair.
 */
static struct file_pair *compare_files(struct file_database *db)
{

	struct file_node *curr, *file_ptr = db->first_node;
	struct file_pair *first_pair;
	struct file_pair **fp = &first_pair;
	int i = 0;
	int k = 0;

	while (file_ptr != NULL) {
		curr = file_ptr->next;
		while (curr != NULL) {
			*fp = malloc(sizeof(struct file_pair));
			(*fp)->file1 = file_ptr;
			(*fp)->file2 = curr;
			//create_mean_list(&fp);
			curr = curr->next;
			fp = &(*fp)->next;
			k++;
		}
		file_ptr = file_ptr->next;
	}

	if (first_pair == NULL)
		printf("SHIT\n");

	for (fp = &first_pair; *fp != NULL; fp = &(*fp)->next) {
		i++;

	}
	printf("Times looped: %d\nNumber of nodes: %d\n", k, i);
	printf("How the fuck is that possible?\n");
	return first_pair;
}

/*
 * Purposse: Helper function for sort_files that uses an insertion sort algorithm.
 * Return value: None.
 */
static void insertion_sort(struct file_node **head, struct file_node *new_node)
{
	struct file_node *curr;

	if (*head == NULL || (*head)->num_words >= new_node->num_words) {
		new_node->next = *head;
		*head = new_node;
		return;
	}

	curr = *head;
	while (curr->next != NULL) {
		if (new_node->num_words > curr->next->num_words) {
			curr = curr->next;
		} else {
			break;
		}
	}
	new_node->next = curr->next;
	curr->next = new_node;
}

/*
 * Purpose: Sorts all files in the database by the number of tokens,
 * from least to greatest.
 * Return value: None.
 */
static void sort_files(struct file_database *db)
{
	struct file_node *file_ptr = db->first_node;
	struct file_node *sorted_list = NULL;
	struct file_node *next_node;

	while (file_ptr != NULL) {
		next_node = file_ptr->next;
		insertion_sort(&sorted_list, file_ptr);
		file_ptr = next_node;
	}

	db->first_node = sorted_list;
}

int main(int argc, char **argv)
{
	struct thread_data *t_data;
	struct file_database *db;
	struct file_pair *pair_list;
	char *parent_dir;
	DIR *dir;
	UNUSED(argc);

	db = new_db();
	parent_dir = parent_dir_alloc(argv[1]);
	dir = opendir(parent_dir);

	if (!dir) {
		errx(1, "No such directory found.");
	} else {
		closedir(dir);
	}

	t_data = new_thread_data(db, parent_dir);
	start_dirhandler(t_data);
	/* At this point, all newly spawned threads have finished. */

	if (db->first_node == NULL)
		errx(1, "No files in the database.");
	if (db->first_node->next == NULL)
		errx(1, "Only 1 file in the database.");

	print_db(db);
	sort_files(db);
	print_db(db);

	pair_list = compare_files(db);

	free_database(db);
	return 0;
}
