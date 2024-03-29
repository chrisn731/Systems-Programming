#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dirhandler.h"
#include "data.h"

/* Color codes */
#define RED	"\033[0;31m"
#define GREEN	"\033[0;32m"
#define YELLOW	"\033[0;33m"
#define BLUE	"\033[0;34m"
#define CYAN	"\033[0;36m"
#define RESET	"\033[0m"

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
	double JSD;
	unsigned int num_words;
};

/*
 * Purpose: Mallocs space for a word_info node.
 * Return value: Pointer to a word_info node.
 */
static struct word_info *create_word_info(char *word, struct file_word *w1, struct file_word *w2)
{
	struct word_info *fw;

	if (!(fw = malloc(sizeof(*fw))))
		err(-1, "Memory alloc error.");
	fw->file1_prob = w1 != NULL ? w1->prob : 0;
	fw->file2_prob = w2 != NULL ? w2->prob : 0;
	fw->mean = (fw->file1_prob + fw->file2_prob) / 2;
	fw->word = word;
	fw->next = NULL;
	return fw;
}

/*
 * Purpose: Allocates space for a pair of files.
 * Return value: Pointer to a file_pair struct.
 */
static struct file_pair *create_file_pair(struct file_node *f1, struct file_node *f2)
{
	struct file_pair *fp;

	if (!(fp = malloc(sizeof(struct file_pair))))
		err(-1, "Memory alloc error.");
	fp->file1 = f1;
        fp->file2 = f2;
	fp->first_word_info = NULL;
	fp->next = NULL;

	return fp;
}

/*
 * Purpose: Free word_info list.
 * Return Value: None.
 */
static void free_word_info_list(struct word_info *p)
{
	struct word_info *temp;
	while (p) {
		temp = p->next;
		free(p);
		p = temp;
	}
}

/*
 * Purpose: Free file pair list.
 * Return Value: None.
 */
static void free_file_pair_list(struct file_pair *head)
{
	struct file_pair *temp;
	while (head) {
		temp = head->next;
		free_word_info_list(head->first_word_info);
		free(head);
		head = temp;
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
	if (!parent_dir)
		err(-1, "Memory alloc error");
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
	double pr1, pr2;
	double KLD1 = 0;
        double KLD2 = 0;

	for (wi = fp->first_word_info; wi; wi = wi->next) {
		pr1 = wi->file1_prob;
		pr2 = wi->file2_prob;
		if (pr1 != 0.0)
			KLD1 += pr1 * (log(pr1/wi->mean) / log(10));
		if (pr2 != 0.0)
			KLD2 += pr2 * (log(pr2/wi->mean) / log(10));
	}
	return (KLD1 + KLD2) / 2;
}

/*
 * Purpose: Find similar words between two file word trees. If a similar word
 * is found, or they don't share a certain word, create a new word info node.
 * Return Value: None.
 */
static void find_similar_words(struct file_word *tree1, struct file_word *tree2,
				struct word_info ***info_node)
{
	struct file_word *b_root;
	int strcmp_ret;

	/* Preorder Traversal */
	if (tree1) {
		b_root = tree2;
		while (tree2) {
			strcmp_ret = strcmp(tree1->word, tree2->word);
			if (strcmp_ret == 0) {
				**info_node = create_word_info(tree1->word, tree1, tree2);
				break;
			} else if (strcmp_ret > 0) {
				/* Tree1->word > tree2->word */
				tree2 = tree2->right;
			} else {
				/* Tree1->word < tree2->word */
				tree2 = tree2->left;
			}
		}
		if (!tree2)
			**info_node = create_word_info(tree1->word, tree1, NULL);
		*info_node = &(**info_node)->next;
		find_similar_words(tree1->left, b_root, info_node);
		find_similar_words(tree1->right, b_root, info_node);
	}
}

/*
 * Purpose: Compare the words in tree 2 to the word info list. If a word is
 * in tree 2, but not in the word info list, append it to the list.
 * Return Value: None.
 */
static void cmp_file_info_list(struct file_word *tnode, struct word_info *list,
				struct word_info ***curr)
{
	struct word_info *ptr;

	/* Preorder Traversal */
	if (tnode) {
		for (ptr = list; ptr; ptr = ptr->next) {
			if (strcmp(tnode->word, ptr->word) == 0)
				break;
		}
		if (!ptr) {
			**curr = create_word_info(tnode->word, NULL, tnode);
			*curr = &(**curr)->next;
		}
		cmp_file_info_list(tnode->left, list, curr);
		cmp_file_info_list(tnode->right, list, curr);
	}
}

/*
 * Purpose: Creates a list of "information nodes" from the token distributions
 * found from both files in a file pair. Each node contains the individual
 * probabilities of a particular word appearing in either file and the mean
 * probability of that word of the file pair.
 * Return value: None.
 */
static void create_info_list(struct file_pair *fp)
{
	struct word_info *first_word_info = NULL;
	struct word_info **info_node = &first_word_info;

	find_similar_words(fp->file1->word, fp->file2->word, &info_node);
	cmp_file_info_list(fp->file2->word, first_word_info, &info_node);
	fp->num_words = fp->file1->num_words + fp->file2->num_words;
	fp->first_word_info = first_word_info;
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

	for (file_ptr = db->first_node; file_ptr; file_ptr = file_ptr->next) {
		for (curr = file_ptr->next; curr; curr = curr->next) {
			*fp = create_file_pair(file_ptr, curr);
			create_info_list(*fp);
			fp = &(*fp)->next;
		}
	}
	return first_pair;
}

/*
 * Purposse: Helper function for sort_files that uses an insertion sort algorithm.
 * Return value: None.
 */
static void insertion_sort(struct file_node **head, struct file_node *new_node)
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

/*
 * Purpose: Check to see if the directory that the user
 * passes in exists/valid/accessible.
 * Return Value: None.
 */
static void check_for_dir(const char *path)
{
	DIR *dirp;
	if (!(dirp = opendir(path))) {
		if (errno == EACCES)
			errx(1, "Cannot access '%s'", path);
		else if (errno == ENOTDIR)
			errx(1, "'%s' is not a directory.", path);
		else if (errno == ENOENT)
			errx(1, "'%s' does not exist.", path);
		else
			errx(1, "Error while accessing '%s'.", path);
	}
	closedir(dirp);
}

/*
 * Purpose: Iterate through the file_pair list and calculate the JSD of each
 * pair.
 * Return Value: None.
 */
static void get_JSD_values(struct file_pair *head)
{
	for (; head; head = head->next)
		head->JSD = compute_JSD(head);
}

/*
 * Purpose: Iterate through the file_pair list and print out the JSD of each
 * pair, with corresponding colors for the value.
 * Return Value: None.
 */
static void print_values(const struct file_pair *ptr)
{
	for (; ptr; ptr = ptr->next) {
		double JSD = ptr->JSD;
		const char *color;
		if (JSD <= 0.1)
			color = RED;
		else if (JSD <= 0.15)
			color = YELLOW;
		else if (JSD <= 0.2)
			color = GREEN;
		else if (JSD <= 0.25)
			color = CYAN;
		else if (JSD <= 0.3)
			color = BLUE;
		else
			color = RESET; /* Should never happen */

		printf("%s%f%s ", color, JSD, RESET);
		printf("\"%s\" and \"%s\"\n", ptr->file1->filepath, ptr->file2->filepath);
	}
}

int main(int argc, char **argv)
{
	struct thread_data *t_data;
	struct file_database *db;
	struct file_pair *pair_list;
	char *parent_dir;

	if (argc != 2)
		errx(1, "Usage: ./detector <path to directory>");

	db = new_db();
	parent_dir = parent_dir_alloc(argv[1]);
	check_for_dir(parent_dir);
	t_data = new_thread_data(db, parent_dir);

	/* Start the scan of the directory, recursively down. */
	start_dirhandler(t_data);
	/* At this point, all newly spawned threads have finished. */

	if (db->first_node == NULL)
		errx(1, "No files found.");
	if (db->first_node->next == NULL)
		errx(1, "Only 1 file found.");

	sort_files(db);
	pair_list = compare_files(db);
	get_JSD_values(pair_list);
	sort_pairs(&pair_list);
	print_values(pair_list);

	free_file_pair_list(pair_list);
	free_database(db);
	return 0;
}
