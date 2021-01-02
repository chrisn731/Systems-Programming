#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* If a jokes.txt file is not found, these setup and punch lines will be used */
#define STD_SETUP "Who."
#define STD_PUNCH "I didn't know you were an owl!"

/* Server backlog queue size */
#define BACKLOG 2

struct joke {
	const char *setup;
	const char *punch;
};

/* Error flags for when something goes wrong */
enum err_codes {
	/* Section error codes */
	KNOCK_ERR = (1 << 0),
	WHO_ERR   = (1 << 1),
	SETUP_ERR = (1 << 2),
	SRESP_ERR = (1 << 3),
	PUNCH_ERR = (1 << 4),
	ADS_ERR   = (1 << 5),
	SECTION_ERRS = (KNOCK_ERR | WHO_ERR | SETUP_ERR | SRESP_ERR | PUNCH_ERR | ADS_ERR),

	/* Message Error codes */
	CONT_ERR  = (1 << 6),
	LGTH_ERR  = (1 << 7),
	FRMT_ERR  = (1 << 8),
	MSG_ERRS  = (CONT_ERR | LGTH_ERR | FRMT_ERR),

	/* Connection errors */
	CLNT_ERR  = (1 << 9),
	CONN_ERR  = (1 << 10),
};

static int read_data(int, void *, unsigned int);
static int write_data(int, const void *, unsigned int);

/*
 * Purpose: Make sure that the user is actually passing in
 * a valid port. AKA not something like 8=03e32.
 * Return Value: 0 on success, non zero on failure.
 */
static int sanatize(const char *ptr)
{
	for (; *ptr != '\0'; ++ptr) {
		if (!isdigit(*ptr))
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
	const char *desc;
	char msg_num, err_code[5] = {'M'};

	/* Check to see if we lost connection to the remote host. */
	if (err_flags & CONN_ERR) {
		warnx("Connection to remote host lost.");
		return;
	}

	/* check if the client sent us an error message */
	if (err_flags & CLNT_ERR) {
		/* Read in the next 4 bytes for the error code */
		if (read_data(cfd, err_code, 4) < 0) {
			warnx("Remote host sent fragmented error."
						"Closing Connection...");
			return;
		}
	} else {
	/* Client sent us bad message. Create an error code, and handle it */
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
	warnx("[ERR] %s - message %c %s", err_code, msg_num, desc);
}

/*
 * Purpose: Convert a number to a string. (i.e. 23 => "23\0")
 * Return Value: Pointer to number string.
 */
static char *itoa(int to_conv, int *length)
{
	int num_digits, y = to_conv;
	char *buf;

	for (num_digits = 0; to_conv > 0; ++num_digits)
		to_conv /= 10;
	if ((buf = malloc(sizeof(*buf) * (num_digits + 1))) == NULL)
		err(-1, "Memory alloc error");
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
	if (amt != 0) {
		size_t left = amt;
		int n_read;
		char *ptr = buf;
		do {
			n_read = read(fd, ptr, 1);
			if (n_read == -1) {
				err(-1, "Fatal read error.");
			} else if (n_read == 0) {
			/* Client closed the connection */
				return -1;
			} else if (!iscntrl(*ptr)) {
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
	if (amt != 0) {
		size_t left = amt;
		int n_write;
		const char *ptr = buf;
		do {
			n_write = write(fd, ptr, left);
			if (n_write == -1) {
				err(-1, "Fatal write error.");
			} else if (n_write == 0) {
				return -1;
			} else {
				ptr += n_write;
				left -= n_write;
			}
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
	if (strcmp("ERR|", buffer) == 0)
		return CLNT_ERR;
	if (strcmp(correct_header, buffer) != 0)
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
	int n_read, err_flag = 0, bytes_read = 0;
	char byte, *buffer, *save;

	if (!(buffer = malloc(sizeof(*buffer) * (payload_length + 1))))
		err(-1, "Memory alloc error.");
	memset(buffer, 0, payload_length + 1);
	save = buffer;

	/* Read in the payload until we find '|' '?' '!' or '.' */
	do {
		if ((n_read = read_data(fd, &byte, 1)) < 0) {
			err_flag = CONN_ERR;
			goto free_exit;
		}
		if (byte == '|')
			break;
		bytes_read += n_read;
		*save++ = byte;
	} while (!is_punct(byte) && bytes_read < payload_length);

	/* If we read more or less bytes than promised set err_flag */
	if (bytes_read != payload_length) {
		err_flag = LGTH_ERR;
		goto free_exit;
	}

	/* If we read punctuation, ensure the next character is the ending '|' */
	if (byte != '|') {
		if (read_data(fd, &byte, 1) < 0) {
			err_flag = CONN_ERR;
			goto free_exit;
		}
		/*
		 * If after another read we get no pipe, then
		 * the user gave too many bytes.
		 */
		if (byte != '|') {
			err_flag = LGTH_ERR;
			goto free_exit;
		}
	}

	/*
	 * If we should be comparing to another string, compare.
	 * Else, just make sure punctuation is there.
	 */
	if (content) {
		if (strncmp(content, buffer, strlen(content)))
			err_flag = CONT_ERR;
	} else {
		if ((strlen(buffer) == 0) || !is_punct(buffer[strlen(buffer) - 1]))
			err_flag = CONT_ERR;
	}
	printf("\t%s <\n", buffer);
	/* Free the buffer and return error flag (0 if no errors have occured) */
free_exit:
	free(buffer);
	return err_flag;
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
	int payload_length, err_flag = 0;
	const char expected[] = "Who's there?";

	if (((err_flag = read_header(cfd)) ||
	     (err_flag = read_payload_length(cfd, &payload_length))) == 0) {
		/* sizeof - 1 because '\0' isnt part of the payload */
		if (payload_length != (sizeof(expected) - 1))
			err_flag = LGTH_ERR;
		else
			err_flag = read_payload(cfd, payload_length, expected);
	}
	return err_flag ? err_flag | WHO_ERR : 0;
}

/*
 * Purpose: Read in the client's response to our sent setup message.
 * If it is differnt in any way from what we expected, report the error.
 * Return Value: 0 for success, nonzero otherwise.
 */
static int recv_setup_resp(int cfd, const char *setup)
{
	int payload_length, full_msg_size, err_flag;
	char *full_msg;

	if ((err_flag = read_header(cfd)) ||
	    (err_flag = read_payload_length(cfd, &payload_length)))
		goto err_out;

	full_msg_size = strlen(setup) + sizeof(" who?");
	/* full_msg_size - 1 to account for '\0' not being part of payload */
	if (payload_length != (full_msg_size - 1)) {
		err_flag = LGTH_ERR;
	} else {
		if (!(full_msg = malloc(sizeof(*full_msg) * full_msg_size)))
			err(-1, "Memory alloc error");
		/* strlen -1 so we dont to include our punctuation */
		sprintf(full_msg, "%.*s, who?", (int) strlen(setup) - 1, setup);
		err_flag = read_payload(cfd, payload_length, full_msg);
		free(full_msg);
	}
err_out:
	return err_flag ? err_flag | SRESP_ERR : 0;
}

/*
 * Purpose: Send a message to the client in the correct format.
 * Return Value: 0 for success, nonzero otherwise.
 */
static int send_resp(int cfd, const char *setup)
{
	int num_len, msg_len, total_len, err_flag = 0;
	char *num, *msg;

	msg_len = strlen(setup);

	/* Turn our length into a string 23 => "23". */
	num = itoa(msg_len, &num_len);

	/*
	 * Get our total length of our new msg:
	 * len = payload_len + num_len + sizeof(REG|||)
	 */
	total_len = msg_len + num_len + sizeof("REG|||");
	if (!(msg = malloc(sizeof(*msg) * total_len)))
		errx(-1, "Out of memory");

	/* Create our msg and send it */
	sprintf(msg, "REG|%s|%s|", num, setup);
	if (write_data(cfd, msg, strlen(msg)) < 0)
		err_flag = CONN_ERR;
	printf("> %s\n", setup);
	free(msg);
	free(num);
	return err_flag;
}

/*
 * Purpose: Read in message of annoyance/disgust/surprise.
 * Check for incorrectly formatted message.
 * Return Value: 0 for success, nonzero otherwise.
 */
static int recv_ads(int cfd)
{
	int payload_length, err_flag = 0;

	if ((err_flag = read_header(cfd)) ||
	    (err_flag = read_payload_length(cfd, &payload_length)) ||
	    (err_flag = read_payload(cfd, payload_length, NULL)))
		return err_flag | ADS_ERR;
	else
		return 0;
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
	int client_fd, err_flag, rand_num;

	srand(time(NULL));
	for (;;) {
		if ((client_fd = accept(sfd, NULL, NULL)) == -1) {
			if (errno == ECONNABORTED)
				continue;
			else
				err(-1, "Fatal accept error.");
		}

		/* Retrieve joke from joke array */
		rand_num = rand();
		rand_num %= arr_len;
		stdjoke = &joke_arr[rand_num];

		/* If an error occurs at any step, abort. */
		if ((err_flag = send_knock_knock(client_fd)) ||
		    (err_flag = recv_whos_there(client_fd)) ||
		    (err_flag = send_resp(client_fd, stdjoke->setup)) ||
		    (err_flag = recv_setup_resp(client_fd, stdjoke->setup)) ||
		    (err_flag = send_resp(client_fd, stdjoke->punch)) ||
		    (err_flag = recv_ads(client_fd)))
			handle_err(client_fd, err_flag);

		if (close(client_fd) == -1)
			err(-1, "Fatal Error closing client connection.");
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

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, port, &hints, &addr_list))
		err(1, "Getaddrinfo error for port %s", port);

	for (addr = addr_list; addr != NULL; addr = addr->ai_next) {
		*sfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (*sfd < 0)
			continue;
		if ((bind(*sfd, addr->ai_addr, addr->ai_addrlen) == 0) &&
			(listen(*sfd, BACKLOG) == 0)) {
			break;
		}
		if (close(*sfd) == -1)
			err(-1, "Fatal error closing incomplete server descriptor.");
	}
	if (!addr)
		errx(1, "Couldn't bind on %s", port);
	freeaddrinfo(addr_list);
}

/*
 * Purpose: Get at least 1024 of one line from fp.
 * Return Value: Malloc'd pointer to read in bytes.
 */
static char *get_file_line(FILE *fp)
{
	char *copy, buffer[1024];

	do {
		if (fgets(buffer, 1024, fp) == NULL)
			err(-1, "Error reading from jokes.txt");
	} while (*buffer == '\n');
	if ((copy = strchr(buffer, '\n')) != NULL)
		*copy = '\0';
	if (!(copy = malloc(sizeof(*copy) * (strlen(buffer) + 1))))
		err(-1, "Memory alloc error.");
	strcpy(copy, buffer);
	return copy;
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
	struct joke *new_arr;
	int joke_lines = 0, err_flag = 0;
	char *buffer;

	if (!(fp = fopen("jokes.txt", "r"))) {
		warnx("Jokes.txt not found, using 1 joke.");
		return -1;
	}

	/* Just need enough room to clear a whole line */
	if (!(buffer = malloc(sizeof(*buffer) * 1024)))
		err(-1, "Memory error");

	/* Count how many lines there are in the file that have jokes */
	while (fgets(buffer, 1024, fp)) {
		if (*buffer != '\n')
			joke_lines++;
	}
	free(buffer);

	/*
	 * Ensure that:
	 * 	1. Jokes file isn't empty.
	 * 	2. Jokes file doesn't have an odd number of filled lines.
	 * If those are satisifed, retrieve jokes from file.
	 */
	if (joke_lines == 0) {
		warnx("No jokes found in jokes.txt, using 1 joke.");
		err_flag = -1;
	} else if ((joke_lines % 2) != 0) {
		warnx("Jokes.txt format corrupted, using 1 joke.");
		err_flag = -1;
	} else {
		/* joke lines =  number of complete jokes (setup line + punch) */
		joke_lines >>= 1;

		if (!(new_arr = malloc(sizeof(*new_arr) * joke_lines)))
			err(-1, "Memory error.");
		*arr_len = joke_lines;
		*joke_arr = new_arr;

		rewind(fp);
		for (; joke_lines > 0; joke_lines--, new_arr++) {
			new_arr->setup = get_file_line(fp);
			new_arr->punch = get_file_line(fp);
		}
	}
	if (fclose(fp))
		err(-1, "Error closing jokes.txt");
	return err_flag;
}

int main(int argc, char **argv)
{
	struct joke *jokes = NULL;
	int server_fd, port, jarray_len;

	if (argc != 2)
		errx(1, "Usage: %s <port number>", argv[0]);
	if (sanatize(argv[1]))
		errx(1, "Why you entering characters my man");
	port = atoi(argv[1]);
	if (port < 5000 || port > 65536)
		errx(1, "Port number must be between 5000 - 65536");
	if (get_jokes(&jokes, &jarray_len)) {
		if (!(jokes = malloc(sizeof(*jokes))))
			err(-1, "Memory alloc error.");
		jokes->setup = STD_SETUP;
		jokes->punch = STD_PUNCH;
		jarray_len = 1;
	}

	open_server_sock(argv[1], &server_fd);
	handle_connection(server_fd, jokes, jarray_len);

	/* Should never be reached. */
	if (close(server_fd) == -1)
		err(-1, "Error while closing server descriptor.");
	return 0;
}
