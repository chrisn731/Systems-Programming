#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* Convert single integer to char equivalent */
#define itoc(c) ((c) + 48)

/* Server Backlog size */
#define BACKLOG 0

/* Section error codes */
#define KNOCK_ERR 0
#define WHO_ERR   1
#define SETUP_ERR 2
#define SRESP_ERR 4
#define PUNCH_ERR 8
#define ADS_ERR   16

/* Message Error codes */
#define CONT_ERR  32
#define LGTH_ERR  64
#define FRMT_ERR  128
#define CLNT_ERR  256
#define CONN_ERR  512

#define SECTION_ERR (KNOCK_ERR | WHO_ERR | SETUP_ERR | SRESP_ERR | PUNCH_ERR | ADS_ERR)
#define MSG_ERRS (CONT_ERR | LGTH_ERR | FRMT_ERR)

struct joke {
	char *setup;
	char *punch;
};

static int write_data(int, const void *, unsigned int);
static int read_data(int, void *, unsigned int);

static void die(const char *error, ...)
{
	va_list argp;
	va_start(argp, error);
	vfprintf(stderr, error, argp);
	va_end(argp);
	fputc('\n', stderr);
	exit(1);
}

static int sanatize(const char *ptr)
{
	for (; *ptr != '\0'; ++ptr) {
		if (isalpha(*ptr))
			return 1;
	}
	return 0;
}

static void handle_err(int cfd, int err_flags)
{
	char err_code[] = {'M', 0, 0, 0, 0};
	char err_msg[sizeof(err_code) + 5] = {0};

	/* check if the client sent us an error message */
	if (err_flags & CLNT_ERR) {
		sprintf(err_msg, "ERR|");
		/* Read in the rest of the client's error message */
		if (read_data(cfd, err_msg + 3, 5) < 0)
			return;

	/* Check to see if we lost connection to the remote host. */
	} else if (err_flags & CONN_ERR) {
		printf("Connection to remote host lost.\n");
		return;

	/* Client sent us bad message. Handle it. */
	} else {
		switch (err_flags & SECTION_ERR) {
		case KNOCK_ERR:
			err_code[1] = '0';
			break;
		case WHO_ERR:
			err_code[1] = '1';
			break;
		case SETUP_ERR:
			err_code[1] = '2';
			break;
		case SRESP_ERR:
			err_code[1] = '3';
			break;
		case PUNCH_ERR:
			err_code[1] = '4';
			break;
		case ADS_ERR:
			err_code[1] = '5';
			break;
		}
		switch (err_flags & MSG_ERRS) {
		case CONT_ERR:
			err_code[2] = 'C';
			err_code[3] = 'T';
			break;
		case LGTH_ERR:
			err_code[2] = 'L';
			err_code[3] = 'N';
			break;
		case FRMT_ERR:
			err_code[2] = 'F';
			err_code[3] = 'T';
			break;
		}
		sprintf(err_msg, "ERR|%s|", err_code);
		write_data(cfd, err_msg, sizeof(err_msg) - 1);
	}
	printf("::MSG ERR: %s\n", err_msg);
}

static char *itoa(int x, int *length)
{
	int y = x;
	char *buf;
	for (*length = 0; x > 0; ++*length)
		x /= 10;
	buf = malloc(sizeof(*buf) * (*length + 1));
	if (!buf)
		die("Out of memory.");
	sprintf(buf, "%d", y);
	return buf;
}

static int read_data(int fd, void *buf, unsigned int amt)
{
	if (amt) {
		size_t left = amt;
		int n_read;
		char *ptr = buf;
		do {
			n_read = read(fd, ptr, left);
			if (n_read <= 0) {
				if (n_read == 0) {
					return -1;
				} else {
					die("read error");
				}
			}
			ptr += n_read;
			left -= n_read;
		} while (left > 0);
	}
	return amt;
}

static int write_data(int fd, const void *buf, unsigned int amt)
{
	if (amt) {
		size_t left = amt;
		int n_write;
		const char *ptr = buf;
		do {
			n_write = write(fd, ptr, left);
			if (n_write < 0) {
				if (errno == EBADF)
					die("fd is not valid");
				else if (errno == EINTR)
					die("Write interrupted by signal");
				else
					return -1;
			}
			ptr += n_write;
			left -= n_write;
		} while (left > 0);
	}
	return amt;
}

static int read_header(int fd)
{
	char correct_header[] = "REG|";
	char buffer[sizeof(correct_header)] = {0};

	if (read_data(fd, buffer, sizeof(buffer) - 1) < 0)
		return CONN_ERR;
	if (!strcmp("ERR|", buffer))
		return CLNT_ERR;
	if (strcmp(correct_header, buffer))
		return FRMT_ERR;
	return 0;
}

static int read_payload_length(int fd, int *payload_length)
{
	char buffer[128] = {0};
	char *save, byte;

	save = buffer;
	do {
		if (read_data(fd, &byte, 1) < 0)
			return CONN_ERR;
		if (isdigit(byte))
			*save++ = byte;
	} while (isdigit(byte));
	*payload_length = atoi(buffer);
	if (byte != '|')
		return FRMT_ERR;
	return 0;
}

static int is_punct(char c)
{
	return c == '?' || c == '!' || c == '.';
}

static int read_payload(int fd, int payload_length, char *content)
{
	int n_read, err = 0, bytes_read = 0;
	char byte, *buffer, *save;

	buffer = malloc(sizeof(*buffer) * (payload_length + 1));
	memset(buffer, 0, payload_length + 1);
	save = buffer;

	/* Read in the payload until we find '|' '?' '!' or '.' */
	do {
		if ((n_read = read_data(fd, &byte, 1)) < 0) {
			err = CONN_ERR;
			goto free_exit;
		}
		bytes_read += n_read;
		if (bytes_read > payload_length)
			break;
		*save++ = byte;
	} while (!is_punct(byte) && byte != '|');

	/* If we read more or less bytes than promised err */
	if (bytes_read != payload_length) {
		err = LGTH_ERR;
		goto free_exit;
	}

	/* If we read punct before the '|' check for '|' */
	if (byte != '|') {
		if (read_data(fd, &byte, 1) < 0) {
			err = CONN_ERR;
			goto free_exit;
		}
		/*
		 * If after another read we get no pipe, then
		 * the user gave too many bytes.
		 */
		if (byte != '|') {
			err = LGTH_ERR;
			goto free_exit;
		}
	}

	/*
	 * If we should be comparing to another string, compare.
	 * Else, just make sure punctuation is there.
	 */
	if (content) {
		if (strncmp(content, buffer, strlen(content) - 1))
			err = CONT_ERR;
	} else {
		if (!is_punct(buffer[strlen(buffer) - 1]))
			err = CONT_ERR;
	}
free_exit:
	free(buffer);
	return err;
}

static int send_knock_knock(int cfd)
{
	char message[] = "REG|13|Knock, knock.|";
	if (write_data(cfd, message, sizeof(message) - 1) < 0)
		return CONN_ERR;
	return 0;
}

static int recv_whos_there(int cfd)
{
	int payload_length, err = 0;
	char expected[] = "Who's there?";
	if ((err = read_header(cfd)))
		return err;
	if ((err = read_payload_length(cfd, &payload_length)))
		return err;
	if (payload_length != (sizeof(expected) - 1))
		return LGTH_ERR;
	err = read_payload(cfd, payload_length, expected);
	return err;
}

static int recv_setup_resp(int cfd, char *setup)
{
	int payload_length, full_msg_size, err = 0;
	char ending[] = ", who?";
	char *full_msg;

	if ((err = read_header(cfd)))
		return err;
	if ((err = read_payload_length(cfd, &payload_length)))
		return err;

	full_msg_size = strlen(setup) + sizeof(ending) - 1;
	if (payload_length != (full_msg_size - 1))
		return LGTH_ERR;

	full_msg = malloc(sizeof(*full_msg) * full_msg_size);
	if (!full_msg)
		die("out of memory");

	memset(full_msg, 0, full_msg_size);
	/* -1 for so we dont check for our punctuation that we previously sent. */
	sprintf(full_msg, "%.*s, who?", (int) strlen(setup) - 1, setup);
	err = read_payload(cfd, payload_length, full_msg);
	free(full_msg);
	return err;
}


static int send_resp(int cfd, char *setup)
{
	int num_len, msg_len, total_len, err = 0;
	char start[] = "REG|";
	char *num, *msg;

	/* Store the length of our message + 1 for punctuation */
	msg_len = strlen(setup);

	/* Turn our length into a string 23 => "23". */
	num = itoa(msg_len, &num_len);

	/*
	 * Get our total length of our new msg:
	 * len = payload_len + num_len + sizeof(REG|||)
	 */
	total_len = msg_len + num_len + 6;
	msg = malloc(sizeof(*msg) * (total_len + 1));
	memset(msg, 0, total_len + 1);
	/* Create our msg */
	sprintf(msg, "REG|%s|%s|", num, setup);

	if (write_data(cfd, msg, strlen(msg)) < 0)
		err = CONN_ERR;
	free(msg);
	free(num);
	return err;
}

static int recv_ads(int cfd)
{
	int payload_length, err = 0;

	if ((err = read_header(cfd)))
		return err;
	if ((err = read_payload_length(cfd, &payload_length)))
		return err;
	err = read_payload(cfd, payload_length, NULL);
	return err;
}

static void handle_connection(int sfd, struct joke *joke_arr, int arr_len)
{
	struct joke *stdjoke;
	int client_fd, err;
	char bruh1[] = "Yo Pierre you wanna come out here?";
	char bruh2[] = "EEEEEHHHHH.";

	stdjoke = malloc(sizeof(*stdjoke));
	stdjoke->setup = bruh1;
	stdjoke->punch = bruh2;
	srand(time(NULL));
	for (;;) {
		client_fd = accept(sfd, NULL, NULL);

		if (client_fd < 0) {
			if (errno == ECONNABORTED) {
				fprintf(stderr, "Queued connection aborted.\n");
				continue;
			} else {
				die("Fatal accept error");
			}
		}
		if (joke_arr) {
			int rand_num = rand();
			rand_num %= arr_len;
			stdjoke->setup = joke_arr[rand_num].setup;
			stdjoke->punch = joke_arr[rand_num].punch;
		}
		err = 0;
		printf("Accepted Client.\n");

		/* > Knock, knock. */
		if ((err = send_knock_knock(client_fd)))
			goto err_and_close;

		/* Who's there? < */
		if ((err = recv_whos_there(client_fd))) {
			err |= WHO_ERR;
			goto err_and_close;
		}

		/* > (Setup) */
		if ((err = send_resp(client_fd, stdjoke->setup)))
			goto err_and_close;

		/* (Setup), who? < */
		if ((err = recv_setup_resp(client_fd, stdjoke->setup))) {
			err |= SRESP_ERR;
			goto err_and_close;
		}

		/* > (Punchline) */
		if ((err = send_resp(client_fd, stdjoke->punch)))
			goto err_and_close;

		/* (Message of disgust) < */
		if ((err = recv_ads(client_fd)))
			err |= ADS_ERR;

		/* Cleanup used resources */
err_and_close:
		if (err)
			handle_err(client_fd, err);
		close(client_fd);
		printf("Client exited with error code: %d\n", err);
	}

}

static void open_server_sock(char *port, int *sfd)
{
	struct addrinfo hints, *addr_list, *addr;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, port, &hints, &addr_list))
			die("getaddrinfo: %s", port);

	for (addr = addr_list; addr != NULL; addr = addr->ai_next) {
		*sfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (*sfd < 0)
			continue;
		if ((bind(*sfd, addr->ai_addr, addr->ai_addrlen) == 0) &&
			(listen(*sfd, BACKLOG) == 0)) {
			break;
		}
		close(*sfd);
	}
	if (!addr)
		die("Couldn't bind (my man)");
	freeaddrinfo(addr_list);
}

static int get_jokes(struct joke **joke_arr, int *arr_len)
{
	FILE *fp;
	int jokes;
	char buffer[1024];

	if (!(fp = fopen("jokes.txt", "r")))
		return -1;
	for (jokes = 0; fgets(buffer, 1024, fp); ++jokes);
	if ((jokes % 2) != 0)
		return -1;
	jokes /= 2;

	*joke_arr = malloc(sizeof(**joke_arr) * jokes);
	if (!*joke_arr)
		die("Out of memory");
	*arr_len = jokes;

	rewind(fp);
	for (jokes = 0; jokes < *arr_len; ++jokes) {
		struct joke *joke = &(*joke_arr)[jokes];
		int length;

		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, 1024, fp);
		length = strlen(buffer);
		joke->setup = malloc(sizeof(char) * length);
		buffer[sizeof(buffer) - 1] = '\0';
		strncpy(joke->setup, buffer, length);
		joke->setup[length - 1] = '\0';

		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, 1024, fp);
		length = strlen(buffer);
		joke->punch = malloc(sizeof(char) * length);
		buffer[sizeof(buffer) - 1] = '\0';
		strncpy(joke->punch, buffer, length);
		joke->punch[length - 1] = '\0';
	}
	fclose(fp);
	return 0;
}

int main(int argc, char **argv)
{
	struct joke *jokes;
	int server_fd, jarray_len;

	if (argc != 2)
		die("Enter a port number my man");
	if (sanatize(argv[1]))
		die("Why you entering characters my man?");
	if (atoi(argv[1]) <= 0)
		die("Port number must be > 0");
	if (get_jokes(&jokes, &jarray_len))
		fprintf(stderr, "Jokes.txt not found, using 1 joke.\n");

	open_server_sock(argv[1], &server_fd);
	handle_connection(server_fd, jokes, jarray_len);

	close(server_fd);
	return 0;
}
