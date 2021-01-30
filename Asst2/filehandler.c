#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "data.h"
#include "filehandler.h"

/*
 * Purpose: Inserts a word into a file's word binary tree. Height of the tree
 * will be log2(num unique words in file). If the word is already in the tree,
 * it's frequency count is incremented. Finally, the number of words in the
 * file is also incremented.
 * Return Value: None.
 */
static void insert_word_entry(struct file_node *file, char *word_to_insert)
{
	struct file_word *new, **parser = &file->word;
	int strcmp_ret;

	file->num_words++;
	while (*parser) {
		strcmp_ret = strcmp((*parser)->word, word_to_insert);
		if (strcmp_ret == 0) {
			/* Our word alredy exists in our list */
			(*parser)->count++;
			free(word_to_insert);
			return;
		} else if (strcmp_ret > 0) {
			parser = &(*parser)->left;
		} else {
			parser = &(*parser)->right;
		}
	}
	new = new_word(word_to_insert);
	*parser = new;
}

/*
 * Purpose: Creates an entry in the file database of a file. Given this
 * function is mutating a shared data structure, it will lock a mutex
 * and then execute it's main purpose.
 * Return value: Pointer to new file node entry.
 */
static struct file_node *create_file_entry(struct file_database *db, char *pathname)
{
	struct file_node *new_entry, **parser = &db->first_node;
	pthread_mutex_t *db_mut = &db->mut;

	new_entry = new_file(pathname);

	pthread_mutex_lock(db_mut);
	while (*parser)
		parser = &(*parser)->next;
	new_entry->next = *parser;
	*parser = new_entry;
	pthread_mutex_unlock(db_mut);
	return new_entry;
}

/*
 * Purpose: Handles opening files. Prints error message and exits
 * calling thread if something were to go wrong.
 * Return Value: Int of file descriptor.
 */
static int open_file(const char *filepath)
{
	int fd;

	if ((fd = open(filepath, O_RDONLY)) < 0)
		warn("Cannot open '%s'", filepath);
	return fd;
}

/*
 * Purpose: Detects whether a character is valid for storing as a word.
 * Return Value: 0 if non valid, non zero otherwise.
 */
static inline int valid_char(char c)
{
	return isalpha(c) || c == '-';
}

/*
 * Purpose: Retrieves a single word from a file. After word is retrieved hands
 * off the found word to create_word_entry() to log it.
 * Return value: Number of bytes read.
 */
static int get_word(int fd, struct file_node *file)
{
	int nr, bytes_parsed = 0, valid_letter_count = 0, buf_size = 32;
	char *word, *save, r_byte;

	word = malloc(sizeof(*word) * buf_size);
	if (!word)
		err(-1, "Memory alloc error");

	while ((nr = read(fd, &r_byte, sizeof(r_byte))) > 0) {
		bytes_parsed += nr;
		if (valid_char(r_byte)) {
			word[valid_letter_count++] = tolower(r_byte);
			/*
			 * In the unlikely case we come across a word longer
			 * than 32 bytes, extend the buffer.
			 */
			if (valid_letter_count >= buf_size) {
				buf_size *= 2;
				save = realloc(word, sizeof(*word) * buf_size);
				if (!save)
					err(-1, "Memory realloc error.");
				word = save;
			}
		} else if (isspace(r_byte)) {
			break;
		}
	}
	if (nr == -1)
		err(-1, "Error while parsing %s", file->filepath);
	word[valid_letter_count] = '\0';
	/* Truncate our buffer to match the actual size of the word */
	save = realloc(word, sizeof(*word) * (valid_letter_count + 1));
	if (!save)
		err(-1, "Memory realloc error");
	insert_word_entry(file, save);
	return bytes_parsed;
}

/*
 * Purpose: Finds the size of a file given by a file descriptor.
 * Return value: Size of file.
 */
static long file_size(int fd)
{
	long size;

	if ((size = lseek(fd, 0, SEEK_END)) == -1 ||
	     lseek(fd, 0, SEEK_SET) == -1)
		size = 0;
	return size;
}

/*
 * Purpose: Parses an entire file for words. If a word is found the beginning
 * of it is handed off to get_word to retrieve it.
 * Return value: None.
 */
static void parse_file(int fd, struct file_node *file)
{
	long total_bytes;
	int n_read;
	char r_byte;

	total_bytes = file_size(fd);
	while (total_bytes > 0) {
		if ((n_read = read(fd, &r_byte, sizeof(r_byte))) <= 0)
			break;
		if (valid_char(r_byte)) {
			if (lseek(fd, -1, SEEK_CUR) == -1)
				err(-1, "Error parsing %s", file->filepath);
			n_read = get_word(fd, file);
		}
		total_bytes -= n_read;
	}
}

/*
 * Purpose: Parses through a file's words and updates their appearance frequencies.
 * Return Value: None.
 */
static void update_probabilities(struct file_word *root, unsigned int total_words)
{
	if (root) {
		root->prob = (double) root->count / total_words;
		update_probabilities(root->left, total_words);
		update_probabilities(root->right, total_words);
	}
}

/*
 * Kickoff function of handling a file.
 * Return Value: NULL.
 */
void *start_filehandler(void *data)
{
	struct thread_data *t_data = data;
	struct file_node *new_file;
	int fd;

	if ((fd = open_file(t_data->filepath)) > 0) {
		new_file = create_file_entry(t_data->db_ptr, t_data->filepath);
		parse_file(fd, new_file);
		update_probabilities(new_file->word, new_file->num_words);
		if (close(fd) == -1)
			err(-1, "Error closing %s.", new_file->filepath);
	}
	free(t_data);
	return NULL;
}
