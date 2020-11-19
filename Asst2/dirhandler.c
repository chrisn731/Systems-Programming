#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <string.h>

#include "filehandler.h"
#include "dirhandler.h"
#include "data.h"

#define FILE_IS_PGRAM(file) (!strcmp("detector", (file)))

struct thread_pool {
	pthread_t *thread_pool;
	int num_threads;
};

/*
 * Returns a pointer to a string that represents a file path.
 * Takes in three args the presenet path (ppath), the filename to append
 * and a number 0 or 1 that denotes if the path is a directory or not.
 * New string for a file looks like this: new = ppath/filename
 * New string for a directroy looks like: new = ppath/dirname/
 */
static char *new_path(const char *ppath, const char *filename, int is_dir)
{
	unsigned int new_path_length;
	char *new;

	/* If it is a dir, we need one extra char for the '/' */
	new_path_length = strlen(ppath) + strlen(filename) + (is_dir ? 2 : 1);
	if (!(new = malloc(sizeof(*new) * new_path_length)))
		errx(1, "Out of memory.");
	strcpy(new, ppath);
	strcat(new, filename);
	/* Put the '/' at the end of our new string for directories */
	if (is_dir)
		new[new_path_length - 2] = '/';

	return new;
}

static struct thread_pool *new_pool(int num_threads)
{
	struct thread_pool *new;

	if (!(new = malloc(sizeof(*new))))
		errx(-1, "Out of memory.");
	new->num_threads = num_threads;
	new->thread_pool = malloc(sizeof(pthread_t) * num_threads);
	if (!new->thread_pool)
		errx(-1, "Out of memory");

	return new;
}

static void free_pool(struct thread_pool *pool)
{
	free(pool->thread_pool);
	free(pool);
}

/*
 * Returns non zero if path is not '.' or '..'
 */
static int not_dots(const char *path)
{
	return strcmp(path, "..") && strcmp(path, ".");
}

/* Helper function to make sure we dont traverse into '.' or '..' */
static int valid_dir(struct dirent *de)
{
	return de->d_type == DT_DIR && not_dots(de->d_name);
}

/* Handles opening a new directory. Makes sure no errors are reported. */
static DIR *attempt_opendir(const char *dir_path)
{
	DIR *dirptr;

	if (!(dirptr = opendir(dir_path))) {
		if (errno == EACCES)
			warnx("Can not access %s", dir_path);
		else
			warnx("Unknown error while attempting to access '%s'", dir_path);
		pthread_exit(NULL);
	}
	return dirptr;
}

/* Returns the number of entries in the directory not including '.' && '..' */
static int num_dir_entries(DIR *dirp)
{
	struct dirent *dir_entry;
	int num = 0;

	while ((dir_entry = readdir(dirp)) != NULL) {
		if (valid_dir(dir_entry) ||
			(dir_entry->d_type == DT_REG && !FILE_IS_PGRAM(dir_entry->d_name))) {
				num++;
		}
	}
	rewinddir(dirp);
	return num;
}

static void
setup_dir_thread(pthread_t *thread, struct thread_data *t_data, const char *fname)
{
	char *dirpath;
	struct thread_data *new;

	dirpath = new_path(t_data->filepath, fname, 1);
	new = new_thread_data(t_data->db_ptr, dirpath);
	pthread_create(thread, NULL, start_dirhandler, new);
}

static void
setup_file_thread(pthread_t *thread, struct thread_data *t_data, const char *fname)
{
	char *filepath;
	struct thread_data *new;

	filepath = new_path(t_data->filepath, fname, 0);
	new = new_thread_data(t_data->db_ptr, filepath);
	pthread_create(thread, NULL, start_filehandler, new);
}

/*
 * Parses a directory for directories and regular files.
 * Directories spawn a new thread invoking start_dirhandler().
 * Files spawn a new thread invoking start_filehandler().
 * All other types of files give warning.
 */
static void parse_dir(DIR *dirp, struct thread_pool *threads, struct thread_data *t_data)
{
	pthread_t *pool = threads->thread_pool;
	struct dirent *dir_entry;
	int thread_num = 0;

	while ((dir_entry = readdir(dirp)) != NULL) {
		if (dir_entry->d_type == DT_DIR) {
			/* dir handler */
			if (not_dots(dir_entry->d_name)) {
				setup_dir_thread(&pool[thread_num], t_data, dir_entry->d_name);
				thread_num++;
			}
		} else if (dir_entry->d_type == DT_REG) {
			/* file handler */
			if (!FILE_IS_PGRAM(dir_entry->d_name)) {
				setup_file_thread(&pool[thread_num], t_data, dir_entry->d_name);
				thread_num++;
			}
		} else {
			warnx("'%s' is not a regular file or directory, skipping.", dir_entry->d_name);
		}
	}
}

/*
 * Thread kickoff. Opens directory given by a filepath, and creates
 * a thread pool to be used by parse_dir.
 */
void *start_dirhandler(void *dir_data)
{
	struct thread_pool *threads;
	struct thread_data *t_data = dir_data;
	DIR *dirp;
	int i, num_entries;

	dirp = attempt_opendir(t_data->filepath);

	if ((num_entries = num_dir_entries(dirp)) != 0) {
		threads = new_pool(num_entries);
		parse_dir(dirp, threads, t_data);
		for (i = 0; i < threads->num_threads; i++)
			pthread_join(threads->thread_pool[i], NULL);
		free_pool(threads);
	}
	closedir(dirp);
	return NULL;
}
