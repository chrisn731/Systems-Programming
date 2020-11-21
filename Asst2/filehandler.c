#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>

#include "filehandler.h"
#include "data.h"

/*
 * Purpose: Inserts a word into a file's word list. These words are always
 * added to the end of the list.
 * Return Value: None.
 */
static void insert_fileword(struct file_node *file, struct file_word *new_word)
{
	struct file_word **parser = &file->word;
	char *word_to_insert = new_word->word;
	int strcmp_ret;

	while (*parser != NULL) {
		strcmp_ret = strcmp((*parser)->word, word_to_insert);
		if (strcmp_ret > 0)
			break;
		parser = &(*parser)->next;
	}
	new_word->next = *parser;
	*parser = new_word;
}

/*
 * Purpose: Search a file's word list for a perticular word.
 * Return Value: If word is found, returns a pointer to it, otherwise NULL.
 */
static struct file_word *
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
 * Purpose: Searches the file node's word list to see if the word already exsits.
 * If it does exist increment word count and free word argument,
 * else append it to the list. Finally, increment the file's total word count.
 * Return Value: None.
 */
static void create_word_entry(struct file_node *file, char *word)
{
	struct file_word *word_entry;

	if ((word_entry = search_for_fileword(file, word)) == NULL) {
		word_entry = new_word(word);
		insert_fileword(file, word_entry);
	} else {
		/*
		 * Increment the count and free our newly malloc'd word if it
		 * already exists within our list.
		 */
		word_entry->count++;
		free(word);
	}
	file->num_words++;
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
		if (n_read < 0) {
			warnx("error during read.");
		} else {
			byte += n_read;
			amt -= n_read;
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
		else
			warnx("Error accessing: '%s'", filepath);
		pthread_exit(NULL);
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
	int word_length = 0, alnum_count = 0;
	char *store, *save, r_byte;

	lseek(fd, -1, SEEK_CUR);
	do {
		word_length += read_data(fd, &r_byte, sizeof(r_byte));
		if (isalnum(r_byte))
			alnum_count++;
	} while (!isspace(r_byte));

	if (alnum_count == 0)
		return word_length;

	if (!(store = malloc(sizeof(*store) * (alnum_count + 1))))
		errx(-1, "Out of memory.");
	save = store;
	lseek(fd, -word_length, SEEK_CUR);
	do {
		read_data(fd, &r_byte, sizeof(r_byte));
		if (isalpha(r_byte))
			*store++ = tolower(r_byte);
		else if (isdigit(r_byte))
			*store++ = r_byte;
	} while (!isspace(r_byte));
	*store = '\0';
	create_word_entry(file, save);
	return word_length;
}

/*
 * Purpose: Finds the size of a file given by a file descriptor.
 * Return value: Size of file.
 */
static int file_size(int fd)
{
	int size;

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
	int total_bytes, n_read;
	char r_byte;

	/* Dont add empty files to the list. */
	total_bytes = file_size(fd);
	while (total_bytes > 0) {
		n_read = read_data(fd, &r_byte, sizeof(r_byte));
		if (isalnum(r_byte))
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

	while (parser != NULL) {
		parser->prob = (double) parser->count / total_num_words;
		parser = parser->next;
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

	fd = open_file(t_data->filepath);
	new_file = create_file_entry(t_data->db_ptr, t_data->filepath);
	parse_file(fd, new_file);
	update_probabilities(new_file);

	free(t_data);
	close(fd);
	return NULL;
}
