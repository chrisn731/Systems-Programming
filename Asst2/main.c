#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <err.h>
#include <math.h>

#include "dirhandler.h"
#include "data.h"

#define RED	"\033[0;31m"
#define GREEN    "\033[0;32m"
#define YELLOW    "\033[0;33m"
#define BLUE    "\033[0;34m"
#define CYAN    "\033[0;36m"
#define RESET    "\033[0m"

struct word_info {
	struct word_info *next;
	double mean;
	double file1_prob;
	double file2_prob;
	char *word;
};

struct file_pair {
	struct file_pair *next;
	struct file_node *file1;
	struct file_node *file2;
	struct word_info *first_word_info;
	unsigned int num_words;
	double JSD;
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

/*
 * Purpose: Allocates memory for the starting directory string.
 * Return value: Pointer to string.
 */
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
 * Purpose: Helper function invoked by sort_pairs that uses insertion sort to sort
 * the list.
 * Return value: None.
 */
static void insertion_sort_pairs(struct file_pair **head, struct file_pair *new_node)
{
	while (*head) {
		if ((*head)->num_words >= new_node->num_words)
			break;
		head = &(*head)->next;
	}
	new_node->next = *head;
	*head = new_node;
}

/*
 * Purpose: Sorts all file pairs by their number of combined tokens.
 * Return value: None.
 */
static void sort_pairs(struct file_pair **fp)
{
	struct file_pair *sorted_list = NULL;
	struct file_pair *next_pair;

	while (*fp != NULL) {
		next_pair = (*fp)->next;
		insertion_sort_pairs(&sorted_list, *fp);
		*fp = next_pair;
	}
	*fp = sorted_list;
}

/*
 * Purpose: Computes the Jensen-Shannon Distance (JSD) for a file pair.
 * Return value: JSD value.
 */
static double compute_JSD(struct file_pair *fp)
{
	struct word_info *wi;
	double KLD1 = 0;
        double KLD2 = 0;
	double pr1, pr2, JSD;

	wi = fp->first_word_info;
	while (wi != NULL) {
		pr1 = wi->file1_prob;
		pr2 = wi->file2_prob;
		if (pr1 != 0)
			KLD1 += pr1 * (log(pr1/wi->mean) / log(10));
		if (pr2 != 0)
			KLD2 += pr2 * (log(pr2/wi->mean) / log(10));
		wi = wi->next;
	}
	JSD = (KLD1 + KLD2) / 2;

	return JSD;
}

/*
 * Purpose: Mallocs space for a word_info node.
 * Return value: Pointer to a word_info node.
 */
static struct word_info *create_word_info(char *word, struct file_word *w1, struct file_word *w2)
{
	struct word_info *fw;

	fw = malloc(sizeof(*fw));
	fw->file1_prob = w1 != NULL ? w1->prob : 0;
	fw->file2_prob = w2 != NULL ? w2->prob : 0;
	fw->mean = (fw->file1_prob + fw->file2_prob) / 2;
	fw->word = word;
	fw->next = NULL;

	return fw;
}

/*
 * Purpose: Creates a list of "information nodes" from the token distributions
 * found from both files in a file pair. Each node contains the individual
 * probabilities of a particular word appearing in either file and the mean
 * probability of that word of the file pair.
 * Return value: None.
 */
static void create_info_list(struct file_pair **fp)
{
	struct file_word *file1_word_ptr = (*fp)->file1->word;
	struct file_word *file2_word_ptr;
	struct word_info *first_word_info = NULL;
	struct word_info *ptr, **info_node = &first_word_info;

	while (file1_word_ptr != NULL) {
		file2_word_ptr = (*fp)->file2->word;
		while (file2_word_ptr != NULL) {
			if (!(strcmp(file1_word_ptr->word, file2_word_ptr->word))) {
				*info_node = create_word_info(file1_word_ptr->word, file1_word_ptr, file2_word_ptr);
				file1_word_ptr = file1_word_ptr->next;
				info_node = &(*info_node)->next;
				break;
			} else {
				file2_word_ptr = file2_word_ptr->next;
			}
		}
		if (!file2_word_ptr) {
			*info_node = create_word_info(file1_word_ptr->word, file1_word_ptr, NULL);
			info_node = &(*info_node)->next;
			file1_word_ptr = file1_word_ptr->next;
		}
	}

	/* Now compare file2 to "word info" list. If a word is missing, add it. */
	file2_word_ptr = (*fp)->file2->word;
	while (file2_word_ptr != NULL) {
		ptr = first_word_info;
		while (ptr != NULL) {
			if (strcmp(file2_word_ptr->word, ptr->word) != 0) {
				ptr = ptr->next;
			} else {
				break;
			}
		}
		if (!ptr) {
			*info_node = create_word_info(file2_word_ptr->word, NULL, file2_word_ptr);
			info_node = &(*info_node)->next;
		}
		file2_word_ptr = file2_word_ptr->next;
	}

	(*fp)->num_words = (*fp)->file1->num_words + (*fp)->file2->num_words;
	(*fp)->first_word_info = first_word_info;
}

/*
 * Purpose: Allocates space for a pair of files.
 * Return value: Pointer to a file_pair struct.
 */
static struct file_pair *create_file_pair(struct file_node *f1, struct file_node *f2)
{
	struct file_pair *fp;

	fp = malloc(sizeof(struct file_pair));
	fp->file1 = f1;
        fp->file2 = f2;
	fp->first_word_info = NULL;
	fp->next = NULL;

	return fp;
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

	while (file_ptr != NULL) {
		curr = file_ptr->next;
		while (curr != NULL) {
			*fp = create_file_pair(file_ptr, curr);
			create_info_list(fp);
			curr = curr->next;
			fp = &(*fp)->next;
		}
		file_ptr = file_ptr->next;
	}

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
	struct file_pair *pair_list, *ptr;
	char *parent_dir;
	DIR *dir;
	double JSD;

	if (argc != 2)
		errx(1, "Enter a valid directory path.");

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

	sort_files(db);

	pair_list = compare_files(db);
	for (ptr = pair_list; ptr != NULL; ptr = ptr->next)
		ptr->JSD = compute_JSD(ptr);

	sort_pairs(&pair_list);
	for (ptr = pair_list; ptr != NULL; ptr = ptr->next) {
		JSD = ptr->JSD;
		if (JSD <= .1)
		       printf(RED);
		else if (JSD <=.15)
			printf(YELLOW);
		else if (JSD <= .2)
			printf(GREEN);
		else if (JSD <=.25)
			printf(CYAN);
		else if (JSD <= .3)
			printf(BLUE);
		printf("%f ", JSD);
		printf(RESET);
		printf("\"%s\" and \"%s\"\n", ptr->file1->filepath, ptr->file2->filepath);
	}

	free_database(db);
	return 0;
}
