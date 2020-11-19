#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <sys/types.h>
#include <ctype.h>

#include "filehandler.h"
#include "data.h"


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

static void create_word_entry(struct file_node *file, char *word)
{
	struct file_word *word_entry;

	if ((word_entry = search_for_fileword(file, word)) == NULL) {
		word_entry = new_word(word);
		insert_fileword(file, word_entry);
	} else {
		word_entry->count++;
	}
	file->num_words++;
}

/*
 * Safe way to read data. Keep attempting to read if our requested
 * value is not given to us from our first read.
 */
static int read_data(int fd, void *buf, size_t amt)
{
	int n_read, save_amt = amt;
	char *byte = buf;
	do {
		n_read = read(fd, byte, amt);
		if (n_read < 0)
			warnx("error during read.");
		byte += n_read;
		amt -= n_read;
	} while (amt > 0);
	return save_amt;
}

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

	store = malloc(sizeof(*store) * alnum_count);
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

static int file_size(int fd)
{
	int size;

	if ((size = lseek(fd, 0, SEEK_END)) < 0)
		size = 0;
	lseek(fd, 0, SEEK_SET);
	return size;
}

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

static void update_probabilities(struct file_node *file)
{
	struct file_word *parser = file->word;
	unsigned int total_num_words = file->num_words;

	while (parser != NULL) {
		parser->prob = (double) parser->count / total_num_words;
		parser = parser->next;
	}
}

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
