#include <ctype.h>
#include <err.h>
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
#define BACKLOG 2

/* Section error codes */
#define KNOCK_ERR 1
#define WHO_ERR   2
#define SETUP_ERR 4
#define SRESP_ERR 8
#define PUNCH_ERR 16
#define ADS_ERR   32

/* Message Error codes */
#define CONT_ERR  64
#define LGTH_ERR  128
#define FRMT_ERR  256
#define CLNT_ERR  512
#define CONN_ERR  1024

#define SECTION_ERRS (KNOCK_ERR | WHO_ERR | SETUP_ERR | SRESP_ERR | PUNCH_ERR | ADS_ERR)
#define MSG_ERRS (CONT_ERR | LGTH_ERR | FRMT_ERR)

struct joke {
	char *setup;
	char *punch;
};

static int write_data(int, const void *, unsigned int);
static int read_data(int, void *, unsigned int);

/*
 * Purpose: Make sure that the user is actually passing in
 * a valid port. AKA not something like 803e32.
 * Return Value: 0 on success, non zero on failure.
 */
static int sanatize(const char *ptr)
{
	for (; *ptr != '\0'; ++ptr) {
		if (isalpha(*ptr))
			return 1;
	}
	return 0;
}

/*
 * Purpose: Handle errors. If a client has sent us an error code,
 * print it out and return. If the client sent us a bad message,
 * create an error message, send it to the client, and print it
 * out to the terminal.
 * Return Value: None.
 */
static void handle_err(int cfd, int err_flags)
{
	char err_code[5] = {'M'};
	const char *desc;
	char msg_num;

	/* check if the client sent us an error message */
	if (err_flags & CLNT_ERR) {
		/* Read in the next 4 bytes for the error code */
		if (read_data(cfd, err_code, 4) < 0)
			return;

	/* Check to see if we lost connection to the remote host. */
	} else if (err_flags & CONN_ERR) {
		warnx("Connection to remote host lost.");
		return;

	/* Client sent us bad message. Handle it. */
	} else {
		char err_msg[sizeof(err_code) + 5] = {0};

		/* Start creating our err msg */
		switch (err_flags & SECTION_ERRS) {
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
		/* send the error to the client */
		sprintf(err_msg, "ERR|%s|", err_code);
		write_data(cfd, err_msg, sizeof(err_msg) - 1);
	}

	/*
	 * Extract some info from the error code to make a useful
	 * error description.
	 */
	msg_num = err_code[1];
	if (err_code[2] == 'C')
		desc = "content was not correct.";
	else if (err_code[2] == 'L')
		desc = "length value was incorrect.";
	else
		desc = "format was broken.";
	warnx("[ERR] %s message %c %s", err_code, msg_num, desc);
}

/*
 * Purpose: Convert a number to a string. (i.e. 23 => "23\0")
 * Return Value: Pointer to number string.
 */
static char *itoa(int x, int *length)
{
	int num_digits, y = x;
	char *buf;
	for (num_digits = 0; x > 0; ++num_digits)
		x /= 10;
	buf = malloc(sizeof(*buf) * (num_digits + 1));
	if (!buf)
		errx(-1, "Out of memory.");
	*length = num_digits;
	sprintf(buf, "%d", y);
	return buf;
}

/*
 * Purpose: Safety wrapper for read function.
 * fd: File descriptor to read from.
 * buf: Buffer to write the data to.
 * amt: How much data to read. Function does not return until
 * 	this value is met OR an error has occured.
 * Return Value: amt or a value < 0 representing an error.
 */
static int read_data(int fd, void *buf, unsigned int amt)
{
	if (amt) {
		size_t left = amt;
		int n_read;
		char *ptr = buf;
		do {
			n_read = read(fd, ptr, 1);
			if (n_read <= 0) {
				if (n_read == 0 || errno == ECONNRESET)
					return -1;
				else
					errx(-1, "Fatal Read Error");
			}
			if (!iscntrl(*ptr)) {
				ptr += n_read;
				left -= n_read;
			}
		} while (left > 0);
	}
	return amt;
}

/*
 * Purpose: Saftey wrapper for write function.
 * fd: File descriptor to write to.
 * buf: Buffer is the data used to be written to the file descriptor.
 * amt: How much data to be written. Function does not return until this
 * 	value is met OR an error has occured.
 * Return Value: amt or a value < 0 representing an error.
 */
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
					errx(-1, "fd is not valid");
				else if (errno == EINTR)
					errx(-1, "Write interrupted by signal");
				else
					return -1;
			}
			ptr += n_write;
			left -= n_write;
		} while (left > 0);
	}
	return amt;
}

/*
 * Purpose: Parse the header and make sure it is correct.
 * Return Value: 0 if received correctely formatted header,
 * nonzero otherwise.
 */
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

/*
 * Purpose: Read in the length of the payload. Make sure that
 * this is also correctly formatted.
 * Return Value: 0 for success, non zero otherwise.
 */
static int read_payload_length(int fd, int *payload_length)
{
	char buffer[128] = {0};
	char *save, byte;

	for (save = buffer; save < (buffer + sizeof(buffer) - 1); ++save) {
		if (read_data(fd, &byte, 1) < 0)
			return CONN_ERR;
		if (!isdigit(byte))
			break;
		*save = byte;
	}
	*payload_length = atoi(buffer);
	if (byte != '|')
		return FRMT_ERR;
	return 0;
}

/*
 * Purpose: Checks for end of sentence punctuation.
 * Return Value: Non zero for success, 0 otherwise.
 */
static inline int is_punct(char c)
{
	return c == '?' || c == '!' || c == '.';
}

/*
 * Purpose: Reads in the payload from client.
 * Reads in until: end of sentence punctuation is met or a '|'.
 * Checks for erros such as too many or to little bytes.
 * If we are checking to see if they sent the correct message compare
 * the payload against content.
 * Else, make sure that it is all correctly formatted.
 * Return Value: 0 for success, nonzero otherwise.
 */
static int read_payload(int fd, int payload_length, const char *content)
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
		if (byte == '|')
			break;
		bytes_read += n_read;
		*save++ = byte;
	} while (!is_punct(byte) && bytes_read < payload_length);

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
		if (strncmp(content, buffer, strlen(content)))
			err = CONT_ERR;
	} else {
		if (!is_punct(buffer[strlen(buffer) - 1]))
			err = CONT_ERR;
	}
	printf("\t%s <\n", buffer);
free_exit:
	free(buffer);
	return err;
}

/*
 * Purpose: Send "Knock, knock." to the client in correct format.
 * Return Value: 0 for success, nonzero otherwise.
 */
static int send_knock_knock(int cfd)
{
	const char message[] = "REG|13|Knock, knock.|";
	/* sizeof - 1 to disregard the '\0' char */
	if (write_data(cfd, message, sizeof(message) - 1) < 0)
		return CONN_ERR;
	puts("> Knock, knock.");
	return 0;
}

/*
 * Purpose: Read in expected "Who's there?" message from client.
 * If client does not send us this or a poorly formatted message,
 * report an error.
 * Return Value: 0 for success, nonzero otherwise.
 */
static int recv_whos_there(int cfd)
{
	int payload_length, err = 0;
	const char expected[] = "Who's there?";
	if ((err = read_header(cfd)))
		return err;
	if ((err = read_payload_length(cfd, &payload_length)))
		return err;
	/* sizeof - 1 because '\0' isnt part of the payload */
	if (payload_length != (sizeof(expected) - 1))
		return LGTH_ERR;
	err = read_payload(cfd, payload_length, expected);
	return err;
}

/*
 * Purpose: Read in the client's response to our sent setup message.
 * If it is differnt in any way from what we expected, report the error.
 * Return Value: 0 for success, nonzero otherwise.
 */
static int recv_setup_resp(int cfd, const char *setup)
{
	int payload_length, full_msg_size, err = 0;
	char *full_msg;

	if ((err = read_header(cfd)))
		return err;
	if ((err = read_payload_length(cfd, &payload_length)))
		return err;

	full_msg_size = strlen(setup) + sizeof(" who?");
	/* full_msg_size - 1 to account for '\0' not being part of payload */
	if (payload_length != (full_msg_size - 1))
		return LGTH_ERR;

	if (!(full_msg = malloc(sizeof(*full_msg) * full_msg_size)))
		errx(-1, "out of memory");

	/* strlen -1 so we dont to include our punctuation */
	sprintf(full_msg, "%.*s, who?", (int) strlen(setup) - 1, setup);
	err = read_payload(cfd, payload_length, full_msg);
	free(full_msg);
	return err;
}

/*
 * Purpose: Send a message to the client in the correct format.
 * Return Value: 0 for success, nonzero otherwise.
 */
static int send_resp(int cfd, const char *setup)
{
	int num_len, msg_len, total_len, err = 0;
	char *num, *msg;

	msg_len = strlen(setup);

	/* Turn our length into a string 23 => "23". */
	num = itoa(msg_len, &num_len);

	/*
	 * Get our total length of our new msg:
	 * len = payload_len + num_len + sizeof(REG|||)
	 */
	total_len = msg_len + num_len + sizeof("REG|||");
	msg = malloc(sizeof(*msg) * total_len);
	/* Create our msg */
	sprintf(msg, "REG|%s|%s|", num, setup);

	if (write_data(cfd, msg, strlen(msg)) < 0)
		err = CONN_ERR;
	printf("> %s\n", setup);
	free(msg);
	free(num);
	return err;
}

/*
 * Purpose: Read in message of annoyance/disgust/surprise.
 * Check for incorrectly formatted message.
 * Return Value: 0 for success, nonzero otherwise.
 */
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

/*
 * Purpose: Main function for the server handling connections.
 * The server goes through steps:
 * 	====== 	  Accept Connection 	======
 * 	====== Get joke from joke array ======
 * 	Server 				Client
 * 	====== 				======
 * 	> Knock, knock.
 * 				Who's there? <
 * 	> (Setup Line)
 * 			       (Setup), who? <
 * 	> (Punchline)
 * 			       (Disgust msg) <
 * 	> <Closes conn>
 * 	====  End of message transaction  ====
 *
 * If an error is found at any point during this transaction, the server
 * reports the error to the console, and closes the connection with the
 * remote client.
 *
 * Return Value: This function never returns. Server will forever run
 * unless a fatal error occurs.
 */
static void handle_connection(int sfd, struct joke *joke_arr, int arr_len)
{
	struct joke *stdjoke;
	int client_fd, err;
	/* Only used if jokes.txt is missing */
	char std_setup[] = "Who.";
	char std_punch[] = "I didn't know you were an owl!";

	if (!joke_arr) {
		if (!(stdjoke = malloc(sizeof(*stdjoke))))
			errx(-1, "Out of memory.");
		stdjoke->setup = std_setup;
		stdjoke->punch = std_punch;
	}

	srand(time(NULL));
	for (;;) {
		client_fd = accept(sfd, NULL, NULL);
		if (client_fd < 0) {
			if (errno == ECONNABORTED)
				continue;
			else
				errx(-1, "Fatal accept error");
		}

		if (joke_arr) {
			int rand_num = rand();
			rand_num %= arr_len;
			stdjoke = &joke_arr[rand_num];
		}
		err = 0;

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
		puts("---------------------------------------------------");
	}

}

/*
 * Purpose: Given a port, sets up a sets up a socket for listening on
 * that port. If binding on that port is not possible, reports an error
 * and exits the program. If binding succeeds, stores the socket file descriptor
 * into sfd.
 * Return Value: None.
 */
static void open_server_sock(const char *port, int *sfd)
{
	struct addrinfo hints, *addr_list, *addr;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, port, &hints, &addr_list))
		errx(1, "Getaddrinfo error for port %s", port);

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
		errx(1, "Couldn't bind on %s", port);
	freeaddrinfo(addr_list);
}

/*
 * Purpose: Get jokes from jokes.txt file.
 * Jokes.txt should be formatted like:
 * 		==== Setup Line =====
 * 		==== Punch line =====
 * 		==== Setup Line =====
 * 			...
 * If the file is not found OR not formatted correctly,
 * returns an error and falls back on a single joke.
 * If the file is usable, creates an array of jokes for
 * the server to randomly select from and send to the client.
 *
 * Return Value: 0 for success, nonzero otherwise.
 */
static int get_jokes(struct joke **joke_arr, int *arr_len)
{
	FILE *fp;
	int jokes;
	char buffer[1024];

	if (!(fp = fopen("jokes.txt", "r")))
		return -1;
	/* Count how many lines there are in the file */
	for (jokes = 0; fgets(buffer, 1024, fp); ++jokes)
		;
	if ((jokes % 2) != 0) {
		fclose(fp);
		return -1;
	}
	/* Amount of lines / 2 = number of jokes */
	jokes >>= 1;

	if (!(*joke_arr = malloc(sizeof(**joke_arr) * jokes)))
		errx(-1, "Out of memory");
	*arr_len = jokes;

	rewind(fp);
	for (jokes = 0; jokes < *arr_len; ++jokes) {
		struct joke *joke = &(*joke_arr)[jokes];
		int length;

		/* get the setup line */
		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, 1024, fp);
		buffer[sizeof(buffer) - 1] = '\0';
		length = strlen(buffer);
		joke->setup = malloc(sizeof(char) * length);
		strncpy(joke->setup, buffer, length);
		joke->setup[length - 1] = '\0';

		/* get the punch line */
		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, 1024, fp);
		buffer[sizeof(buffer) - 1] = '\0';
		length = strlen(buffer);
		joke->punch = malloc(sizeof(char) * length);
		strncpy(joke->punch, buffer, length);
		joke->punch[length - 1] = '\0';
	}
	fclose(fp);
	return 0;
}

int main(int argc, char **argv)
{
	struct joke *jokes = NULL;
	int server_fd, port, jarray_len = 0;

	if (argc != 2)
		errx(1, "Usage: %s <port number>", argv[0]);
	if (sanatize(argv[1]))
		errx(1, "Why you entering characters my man");
	port = atoi(argv[1]);
	if (port < 5000 || port > 65536)
		errx(1, "Port number must be between 5000 - 65536");
	if (get_jokes(&jokes, &jarray_len))
		warnx("jokes.txt not found or format corrupted, using 1 joke.");

	open_server_sock(argv[1], &server_fd);
	handle_connection(server_fd, jokes, jarray_len);

	/* Should never be reached. */
	close(server_fd);
	return 0;
}
