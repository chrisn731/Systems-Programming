/*
 * C String Tokenizer.
 * Authors: Christopher Naporlee && Michael Nelli
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
 * 	This will be filled in when we talk.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_letter(char c);

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


int main(int argc, char **argv)
{
	char *arg;

	if (argc < 2)
		return 1;

	/* Parse command line args */
	while ((arg = argv[1]) == NULL) {
		while (1) {
			switch (*++arg) {
				default:
					break;
			}
			break;
		}
		argv++;
	}
}
