#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>
#include <stdarg.h>

#define BACKLOG 0
#define KNOCK_ERR 0
#define WHO_ERR   1
#define SETUP_ERR 2
#define SRESP_ERR 3
#define PUNCH_ERR 4
#define ADS_ERR   5
#define itoc(c) ((c) + 48)
#define CONT_ERR  6
#define LGTH_ERR  7
#define FRMT_ERR  8

static int sanatize(char *ptr)
{
	for (; *ptr != '\0'; ptr++) {
		if (isalpha(*ptr))
			return 1;
	}
	return 0;
}

static void die(char *error, ...)
{
	va_list argp;
	va_start(argp, error);
	vfprintf(stderr, error, argp);
	va_end(argp);
	fputc('\n', stderr);
	exit(1);
}

static void handle_err(int type, int part)
{
	char err[] = {'M', 0, 0, 0, 0};
	err[1] = itoc(part);
	switch (type) {
	case CONT_ERR:
		err[2] = 'C';
		err[3] = 'T';
		break;
	case LGTH_ERR:
		err[2] = 'L';
		err[3] = 'N';
		break;
	case FRMT_ERR:
		err[2] = 'F';
		err[3] = 'T';
		break;
	}
	die("ERR|%s|", err);
}

static char *itoa(int x, int *length)
{
	int y = x;
	char *buf;
	for (*length = 0; x > 0; ++*length)
		x /= 10;
	buf = malloc(sizeof(*buf) * (*length + 1));
	if (!buf)
		die("Major L");
	sprintf(buf, "%d", y);

	return buf;
}

static size_t read_data(int fd, void *buf, size_t amt)
{
	if (amt) {
		size_t left = amt;
		int n_read;
		char *ptr = buf;
		do {
			n_read = read(fd, ptr, left);
			if (n_read < 0)
				die("write error");
			ptr += n_read;
			left -= n_read;
		} while (left > 0);
	}
	return amt;
}

static size_t write_data(int fd, void *buf, size_t amt)
{
	if (amt) {
		size_t left = amt;
		int n_write;
		char *ptr = buf;
		do {
			n_write = write(fd, ptr, left);
			if (n_write < 0)
				die("write error");
			ptr += n_write;
			left -= n_write;
		} while (left > 0);
	}
	return amt;
}

static void send_knock_knock(int cfd)
{
	char message[] = "REG|13|Knock, knock.|";
	write_data(cfd, message, sizeof(message) - 1);
}


static void recv_whos_there(int cfd)
{
	char msg[128] = {0};
	read_data(cfd, msg, 4);
	if (strcmp(msg, "REG|"))
		die("Does not match");

	memset(msg, 0, sizeof(msg));

	read_data(cfd, msg, 2);
	if (strcmp(msg, "12"))
		die("Wrong numba");
	read_data(cfd, msg, 1);
	if (*msg != '|')
	       die("No Pipe: not chillin");

	memset(msg, 0, sizeof(msg));

	read_data(cfd, msg, 12);
	if (strcmp(msg, "Who's there?"))
		die("Unexpected message");

	read_data(cfd, msg, 1);
	if (*msg != '|')
		die("No Pipe: not chillin");
}

static void send_resp(int cfd, char *setup)
{
	int num_len, msg_len, total_len;
	char *num, *msg;
	char start[] = "REG|";

	msg_len = strlen(setup) + 1;
	num = itoa(msg_len, &num_len);
	total_len = msg_len + num_len + 6;
	msg = malloc(sizeof(*msg) * (total_len + 1));
	memset(msg, 0, total_len + 1);
	strcpy(msg, start);
	strcat(msg, num);
	msg[4 + num_len] = '|';
	strcat(msg, setup);
	msg[total_len - 2] = '.';
	msg[total_len - 1] = '|';
	msg[total_len] = '\0';

	write_data(cfd, msg, strlen(msg));
}

static void recv_setup_resp(int cfd, char *setup)
{
	char exp_resp[100] = {0};
	char msg[128] = {0};
	int setup_resp;

	read_data(cfd, msg, 4);
	if (strcmp(msg, "REG|"))
		die("Does not match");

	memset(msg, 0, sizeof(msg));

	read_data(cfd, msg, 2);
	if (strcmp(msg, itoa(strlen(setup) + 6, &setup_resp)))
		die("Wrong numba");
	read_data(cfd, msg, 1);
	if (*msg != '|')
	       die("No Pipe: not chillin");

	memset(msg, 0, sizeof(msg));
	strcpy(exp_resp, setup);
	strcat(exp_resp, ", who?");

	read_data(cfd, msg, strlen(setup) + 6);
	printf("%s, %s\n", msg, exp_resp);
	if (strcmp(msg, exp_resp))
		die("Unexpected message");

	read_data(cfd, msg, 1);
	if (*msg != '|')
		die("No Pipe: not chillin");
}

static void recv_ads(int cfd)
{
	char msg[128] = {0};
	char byte, *save = msg;
	int msg_len;

	read_data(cfd, msg, 4);
	if (strcmp(msg, "REG|"))
		die("Does not match");

	memset(msg, 0, sizeof(msg));

	do {
		read_data(cfd, &byte, 1);
		if (isdigit(byte))
			*save++ = byte;
	} while (isdigit(byte));

	msg_len = atoi(msg);
	memset(msg, 0, sizeof(msg));
	read_data(cfd, msg, msg_len + 1);
	printf("%s\n", msg);
}

static void handle_connections(int sfd)
{
	int client_fd;
	char setup[] = "Yo Pierre you wanna come out here";
	char punch[] = "EEEEEHHHHH";

	for(; ;) {
		client_fd = accept(sfd, NULL, NULL);

		if (client_fd < 0)
			die("Failed to accept");

		send_knock_knock(client_fd);
		recv_whos_there(client_fd);
		send_resp(client_fd, setup);
		recv_setup_resp(client_fd, setup);
		send_resp(client_fd, punch);
		recv_ads(client_fd);

		close(client_fd);
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

	if (addr == NULL)
		die("Couldn't bind (my man)");

	freeaddrinfo(addr_list);
}

int main(int argc, char **argv)
{
	int server_fd;

	if (argc != 2)
		die("Enter a port number my man");

	if (sanatize(argv[1]) != 0)
		die("Why you entering characters my man?");

	handle_err(FRMT_ERR, PUNCH_ERR);
	open_server_sock(argv[1], &server_fd);
	handle_connections(server_fd);

	close(server_fd);
	return 0;
}
