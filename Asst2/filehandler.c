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
 * Purpose: Inserts a word into a file's word list. These words are always
 * inserted alphabetically. If the word already exists, increments the
 * count of the word. Regardless if the word has a duplicate or not
 * the file total word count is also incremented.
 * Return Value: None.
 */
static void insert_word_entry(struct file_node *file, char *word_to_insert)
{
	struct file_word *new, **parser;
	int strcmp_ret;

	for (parser = &file->word; *parser != NULL; parser = &(*parser)->next) {
		strcmp_ret = strcmp((*parser)->word, word_to_insert)
		if (!strcmp_ret) {
			/* Our word alredy exists in our list */
			(*parser)->count++;
			free(word_to_insert);
			goto done;
		} else if (strcmp_ret > 0) {
			break;
		}
	}
	new = new_word(word_to_insert);
	new->next = *parser;
	*parser = new;
done:
	file->num_words++;
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
	while (*parser != NULL)
		parser = &(*parser)->next;
	new_entry->next = *parser;
	*parser = new_entry;
	pthread_mutex_unlock(db_mut);
	return new_entry;
}

/*
 * Purpose: Read data in from file descriptor and store it into a buffer.
 * Repeat this action until the amount requested has been fulfilled.
 * Return Value: Number of bytes read.
 */
static int read_data(int fd, void *buf, size_t amt)
{
	int n_read, save_amt = amt;
	char *byte = buf;
	do {
		n_read = read(fd, byte, amt);
		if (n_read > 0) {
			byte += n_read;
			amt -= n_read;
		} else if (n_read == 0) {
			*byte = 0;
			return 0;
		} else {
			warnx("error during read.");
		}
	} while (amt > 0);
	return save_amt;
}

/*
 * Purpose: Handles opening files. Prints error message and exits
 * calling thread if something were to go wrong.
 * Return Value: Int of file descriptor.
 */
static int open_file(const char *filepath)
{
	int fd;

	if ((fd = open(filepath, O_RDONLY)) < 0) {
		if (errno == EACCES)
			warnx("Cannot access: '%s'", filepath);
		else if (errno == ENFILE)
			warnx("Limit of open files has been reached.");
		else
			warnx("Error accessing: '%s'", filepath);
	}
	return fd;
}

/*
 * Purpose: Retrieves a single word from a file. After word is retrieved hands
 * off the found word to create_word_entry() to log it.
 * Return value: Number of bytes read.
 */
static int get_word(int fd, struct file_node *file)
{
	int word_length = 0, valid_letter_count = 0;
	char *store, *save, r_byte;

	lseek(fd, -1, SEEK_CUR);
	for (;;) {
		word_length += read_data(fd, &r_byte, sizeof(r_byte));
		if (isspace(r_byte) || r_byte == 0)
			break;
		if (isalpha(r_byte) || r_byte == '-')
			++valid_letter_count;
	}
	if (valid_letter_count == 0)
		return word_length;

	if (!(store = malloc(sizeof(*store) * (valid_letter_count + 1))))
		errx(-1, "Out of memory.");
	save = store;
	lseek(fd, -word_length, SEEK_CUR);
	for (;;) {
		read_data(fd, &r_byte, sizeof(r_byte));
		if (isspace(r_byte) || r_byte == 0)
			break;
		if (isalpha(r_byte) || r_byte == '-')
			*store++ = tolower(r_byte);
	}
	*store = '\0';
	insert_word_entry(file, save);
	return word_length;
}

/*
 * Purpose: Finds the size of a file given by a file descriptor.
 * Return value: Size of file.
 */
static long file_size(int fd)
{
	long size;

	if ((size = lseek(fd, 0, SEEK_END)) < 0)
		size = 0;
	lseek(fd, 0, SEEK_SET);
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

	/* Dont add empty files to the list. */
	total_bytes = file_size(fd);
	while (total_bytes > 0) {
		n_read = read_data(fd, &r_byte, sizeof(r_byte));
		if (isalpha(r_byte) || r_byte == '-')
			n_read = get_word(fd, file);
		total_bytes -= n_read;
	}
}

/*
 * Purpose: Parses through a file's words and updates their appearance frequencies.
 * Return Value: None.
 */
static void update_probabilities(struct file_node *file)
{
	struct file_word *parser = file->word;
	unsigned int total_num_words = file->num_words;

	for (parser = file->word; parser != NULL; parser = parser->next)
		parser->prob = (double) parser->count / total_num_words;
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
		update_probabilities(new_file);
		close(fd);
	}
	free(t_data);
	return NULL;
}
