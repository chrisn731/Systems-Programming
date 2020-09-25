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
#include <ctype.h>

struct input_tokens {
	char **input;
	int num_of_tokens;
};

struct C_token {
	const char *name;
	const char *operator;
};

void die(const char *msg);
void strcopy(const char *src, char *dest, int n);
void print_token(const char *type, const char *tok);
int is_letter(char c);
int is_number(char c);
int is_float (char *str);
int is_hex(char *str);
int num_of_tokens(char *arg);
struct input_tokens *split_tokens(char *arg);

const struct C_token C_tokens[] = {
	{"left parenthesis", "("},
	{"right parenthesis", ")"},
	{"left brace", "["},
	{"right brace", "]"},
	{"structure member", "."},
	{"structure pointer", "->"},
	{"sizeof", "sizeof"},
	{"comma", ","},
	{"negate", "!"},
	{"1s complement", "~"},
	{"shift right", ">>"},
	{"shift left", "<<"},
	{"bitwise XOR", "^"},
	{"bitwise OR", "|"},
	{"increment", "++"},
	{"decrement", "--"},
	{"addition", "+"},
	{"division", "/"},
	{"logical OR", "||"},
	{"logical AND", "&&"},
	{"conditional true", "?"},
	{"conditional false", ":"},
	{"equality test", "=="},
	{"inequality test", "!="},
	{"less than test", "<"},
	{"greater than test", ">"},
	{"less than or equal test", "<="},
	{"greater than or equal test", ">="},
	{"assignment", "="},
	{"plus equals", "+="},
	{"minus equals", "-="},
	{"times equals", "*="},
	{"divide equals", "/="},
	{"mod equals", "%="},
	{"shift right equals", ">>="},
	{"shift left equals", "<<="},
	{"bitwise AND equals", "&="},
	{"bitwise XOR equals", "^="},
	{"bitwise OR equals", "|="},
	{"AND/address operator", "&"},
	{"minus/subtract operator", "-"},
	{"multiply/dereference operator", "*"},
};

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

void print_token(const char *type, const char *token)
{
	printf("%s: \"%s\"\n", type, token);
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
			} while (!isspace(*parse) && *parse != '\0');
			num++;
		}
		while (isspace(*parse) && *parse != '\0') {
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
	tokens->input = malloc(sizeof(*tokens->input) * tokens->num_of_tokens);
	while (1) {
		/* Skip all white space */
		while (isspace(*parser))
			parser++;

		if (*parser == '\0')
			break;

		/* Track the beginning of token and it's length */
		start_of_token = parser;
		strlen = 0;
		while (!isspace(*parser) && *parser != '\0') {
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
	if (argc > 2)
		die("Received too many inputs.");
	if (num_of_tokens(argv[1]) == 0)
		return 0;
	/* Parse command line args */

	return 0;
}
