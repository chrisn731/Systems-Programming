#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "data.h"
#include "dirhandler.h"
#include "filehandler.h"

#define PROGRAM_NAME "detector"

#define FILE_TYPE 0
#define DIR_TYPE  1

/*
 * Purpose: Returns a pointer to a string that represents a file path.
 * Return Value: Pointer to new filepath string.
 * NOTE: Takes in three args the presenet path (ppath), the filename to append
 * and a number 0 or 1 that denotes if the path is a directory or not.
 * New string for a file looks like this: new = ppath/filename
 * New string for a directroy looks like: new = ppath/dirname/
 */
static char *new_path(const char *ppath, const char *filename, int type)
{
	unsigned int new_path_length;
	char *new;

	/* If it is a dir, we need one extra char for the '/' */
	new_path_length = strlen(ppath) + strlen(filename) + (type == DIR_TYPE ? 2 : 1);
	if (!(new = malloc(sizeof(*new) * new_path_length)))
		errx(1, "Out of memory.");
	strcpy(new, ppath);
	strcat(new, filename);
	/* Put the '/' at the end of our new string for directories */
	if (type == DIR_TYPE)
		new[new_path_length - 2] = '/';
	new[new_path_length - 1] = '\0';
	return new;
}

/*
 * Purpose: Allocate memory for a new thread pool.
 * Return Value: Pointer to allocated thread pool.
 */
static pthread_t *new_pool(int num_threads)
{
	pthread_t *new;

	if (!(new = malloc(sizeof(*new) * num_threads)))
		errx(-1, "Out of memory.");
	return new;
}

/*
 * Purpose: Checks if path matches '.' or '..'.
 * Return Value: Returns non zero if path is not '.' or '..'
 */
static int not_dots(const char *path)
{
	return strcmp(path, "..") && strcmp(path, ".");
}

/*
 * Purpose: Helper function to make sure we dont traverse into '.' or '..'
 * Return Value: Non-zero if dir entry (de) is a directory and is not '.' or '..'
 */
static int valid_dir(struct dirent *de)
{
	return de->d_type == DT_DIR && not_dots(de->d_name);
}

/*
 * Purpose: Check if filename matches our running process. Process name
 * is determined by PROGRAM_NAME macro defined above.
 * Return Value: Returns non-zero if the file name does not match the
 * program name. 0 otherwise.
 */
static int file_isnt_program(const char *filename)
{
	return strcmp(PROGRAM_NAME, filename);
}

/*
 * Purpose: Attempts to open a directory. If an error happened, warns the user
 * and exits the calling thread.
 * Return Value: Pointer to directory stream.
 */
static DIR *attempt_opendir(const char *dir_path)
{
	DIR *dirptr;

	if (!(dirptr = opendir(dir_path))) {
		if (errno == EACCES)
			warnx("Can not access '%s'", dir_path);
		else
			warnx("Unknown error while attempting to access '%s'", dir_path);
		pthread_exit(NULL);
	}
	return dirptr;
}

/*
 * Purpose: Count the number of entries in a directory. This count does not
 * include '.', '..', or the program if it is in the directory being searched.
 * Return Value: Count of number of entries.
 */
static int num_dir_entries(DIR *dirp)
{
	struct dirent *dir_entry;
	int num = 0;

	while ((dir_entry = readdir(dirp)) != NULL) {
		if (valid_dir(dir_entry) ||
			(dir_entry->d_type == DT_REG && file_isnt_program(dir_entry->d_name))) {
				num++;
		}
	}
	rewinddir(dirp);
	return num;
}

/*
 * Purpose: Sets up a call to create a new directory handler thread.
 * Return Value: None. Does not return.
 */
static void
setup_dir_thread(pthread_t *thread, struct thread_data *t_data, const char *fname)
{
	char *dirpath;
	struct thread_data *new;

	dirpath = new_path(t_data->filepath, fname, DIR_TYPE);
	new = new_thread_data(t_data->db_ptr, dirpath);
	pthread_create(thread, NULL, start_dirhandler, new);
}

/*
 * Purpose: Sets up a call to create a new file handler thred.
 * Return Value: None. Does not return.
 */
static void
setup_file_thread(pthread_t *thread, struct thread_data *t_data, const char *fname)
{
	char *filepath;
	struct thread_data *new;

	filepath = new_path(t_data->filepath, fname, FILE_TYPE);
	new = new_thread_data(t_data->db_ptr, filepath);
	pthread_create(thread, NULL, start_filehandler, new);
}

/*
 * Purpose: Parses a directory for directories and regular files.
 * All other file types warn the user that it was skipped during the parsing
 * process.
 * Return Value: None.
 * NOTE:
 * Directories spawn a new thread invoking start_dirhandler().
 * Files spawn a new thread invoking start_filehandler().
 */
static void parse_dir(DIR *dirp, pthread_t *pool, struct thread_data *t_data)
{
	struct dirent *dir_entry;

	while ((dir_entry = readdir(dirp)) != NULL) {
		if (dir_entry->d_type == DT_DIR) {
			/* invoke dir handler */
			if (not_dots(dir_entry->d_name))
				setup_dir_thread(pool++, t_data, dir_entry->d_name);
		} else if (dir_entry->d_type == DT_REG) {
			/* invoke file handler */
			if (file_isnt_program(dir_entry->d_name))
				setup_file_thread(pool++, t_data, dir_entry->d_name);
		} else {
			warnx("'%s' is not a regular file or directory, skipping.", dir_entry->d_name);
		}
	}
}

/*
 * Purpose: Directory handler thread kickoff.
 * Opens directory given by a filepath, and creates a thread pool
 * to invoke more threads.
 * Return Value: NULL.
 */
void *start_dirhandler(void *dir_data)
{
	struct thread_data *t_data = dir_data;
	pthread_t *thread_pool;
	DIR *dirp;
	int i, num_entries;

	dirp = attempt_opendir(t_data->filepath);
	/* Skip empty directories */
	if ((num_entries = num_dir_entries(dirp)) != 0) {
		thread_pool = new_pool(num_entries);
		parse_dir(dirp, thread_pool, t_data);
		for (i = 0; i < num_entries; i++)
			pthread_join(thread_pool[i], NULL);
		free(thread_pool);
	}
	closedir(dirp);
	/*
	 * We dont need to save the data (specifically the filepath)
	 * that is passed to our threads. This differs from our
	 * filehandler.
	 */
	free(t_data->filepath);
	free(t_data);
	return NULL;
}
