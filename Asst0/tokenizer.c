/*
 * C String Tokenizer.
 * Authors: Christopher Naporlee && Michael Nelli
 * CS214 Systems Programming | Section 5
 * Program Description:
 * 	This program will take in multiple strings as a command line
 * 	argument. Then it will parse each piece and then return to
 * 	the user what type of argument was passed in.
 *
 * 	For example,
 * 	./tokenizer hello array[123]
 * 	word "hello"
 * 	word "array"
 * 	left brace "["
 * 	integer "123"
 * 	right brace "]"
 */

/*
 * TODO:
 * 	1. Split up given input
 * 	2. Parse all split tokens and make sure there are no combos (123abc)
 * 	3. Kill Allender
 * 	4. Parse all split tokens and print what they are words, numbers, etc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct input_tokens {
	char **input;
	int num_of_tokens;
};

void die(const char *msg);
void strcopy(const char *src, char *dest, int n);
int is_letter(char c);
int is_number(char c);
int is_float (char *str);
int is_hex(char *str);
int num_of_tokens(char *arg);
struct input_tokens *split_tokens(char *arg);

/*
 * Error exit function.
 * Only called if program runs into unrecoverable error.
 * Does not return.
 */
void die(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

/*
 * Similar to strncpy but will insert null terminator
 * at the end of string.
 */
void strcopy(const char *src, char *dest, int n)
{
	while (n > 0) {
		*dest++ = *src++;
		n--;
	}
	*dest = '\0';
}
/*
 * Takes in a single letter 'c' and tests if it is a letter.
 * Returns 1 if it is a letter, 0 otherwise.
 */
int is_letter(char c)
{
	return !!((c >= 'A' && c <= 'Z') ||
		  (c >= 'a' && c <= 'z'));
}

/*
 * Takes in a single letter 'c' and tests if it is a number.
 * Returns 1 if it is a number, 0 otherwise.
 */
int is_number(char c)
{
	return !!(c >= '0' && c <= '9');
}

/*
 * Takes in a string and parses it for a decimal.
 * Returns 1 if decimal found, 0 if none.
 */
int is_float(char *str)
{
	while (*str != '\0') {
		if (*str == '.' && str[1] != '\0') {
			return 1;
		}
		str++;
	}
	return 0;
}

int is_hex(char *str)
{
	return !!((str[0] == '0') &&
		((str[1] == 'x') || str[1] == 'X'));
}

/*
 * Takes in a pointer to a string of tokens and returns num of
 * tokens as int.
 */
int num_of_tokens(char *arg)
{
	int num = 0;
	char *parse = arg;

	while (*parse != '\0') {
		if (*parse != ' ') {
			do {
				parse++;
			} while (*parse != ' ' && *parse != '\0');
			num++;
		}
		while (*parse == ' ' && *parse != '\0') {
			parse++;
		}
	}
	return num;
}

/*
 * Takes in a pointer to a string of tokens and breaks them up by
 * returning pointers to null terminated strings of each token.
 */
struct input_tokens *split_tokens(char *arg)
{
	struct input_tokens *tokens;
	int strlen, index = 0;
	char *start_of_token, *parser = arg;

	tokens = malloc(sizeof(*tokens));
	tokens->num_of_tokens = num_of_tokens(arg);
	if (tokens->num_of_tokens == 0)
		die("Input contains no tokens to be parsed");

	tokens->input = malloc(sizeof(*tokens->input) * tokens->num_of_tokens);
	while (1) {
		/* Skip all white space */
		while (*parser == ' ')
			parser++;

		if (*parser == '\0')
			break;

		/* Track the beginning of token and it's length */
		start_of_token = parser;
		strlen = 0;
		while (*parser != ' ' && *parser != '\0') {
			parser++;
			strlen++;
		}
		/* Allocate space and then copy the token to be used later */
		tokens->input[index] = malloc(sizeof(char) * (strlen + 1));
		strcopy(start_of_token, tokens->input[index], strlen);
		index++;
	}
	return tokens;
}

int main(int argc, char **argv)
{
	if (argc < 2)
		die("Please include a string.");

	/* Parse command line args */

	return 0;
}
