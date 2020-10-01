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
 * 	3. DON'T Kill Allender
 * 	4. Parse all split tokens and print what they are words, numbers, etc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#define DEBUG 0
#define ARRAY_SIZE(X) (int)(sizeof(X)/sizeof(*(X)))

struct input_token {
	char *input;
	struct input_token *next;
};

struct C_token {
	const char *name;
	const char *operator;
};

void die(const char *msg, ...);
void strcopy(const char *src, char *dest, int n);
void print_token(const char *type, const char *tok);
void parse_tokens(struct input_token *);
void sanitize_word(struct input_token **);
void sanitize_num(struct input_token **);
void sanitize_symbol(struct input_token **);
void split_token(struct input_token **, int);
void free_list(struct input_token *);
int is_float (const char *str);
int is_hex(const char *str);
int num_of_tokens(char *arg);
int is_octal(const char *str);
struct input_token *split_tokens(char *arg);
void sanitize_tokens(struct input_token **);

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

const struct C_token C_keywords[] = {
	{"if statement", "if"},
	{"else statement", "else"},
	{"do", "do"},
	{"while", "while"},
	{"for", "for"},
	{"char", "char"},
	{"int", "int"},
	{"double", "double"},
	{"float", "float"},
	{"long", "long"},
	{"short keyword", "short"},
	{"return", "return"},
	{"break", "break"},
	{"continue", "continue"},
	{"const keyword", "const"},
	{"struct", "struct"},
	{"unsigned", "unsigned"},
	{"signed", "signed"},
	{"switch", "switch"},
	{"void", "void"},
	{"case", "case"},
	{"default", "default"},
	{"register", "register"},
	{"typedef", "typedef"},
	{"enum", "enum"},
	{"goto", "goto"},
	{"static", "static"},
	{"union", "union"},
	{"auto", "auto"},
	{"volatile", "volatile"},
	{"extern", "extern"},
	{"sizeof", "sizeof"},
};

/*
 * Fatal error exit function.
 * Only called if program runs into unrecoverable error.
 * Does not return.
 */
void die(const char *err, ...)
{
	va_list argp;
	va_start(argp, err);
	vfprintf(stderr, err, argp);
	va_end(argp);
	fputc('\n', stderr);
	exit(1);
}

/*
 * Copies n characters from src to dest.
 * At n + 1 characters places a null byte.
 */
void strcopy(const char *src, char *dest, int n)
{
	while (n--)
		*dest++ = *src++;

	*dest = '\0';
}

/*
 * Takes in a string and parses it for a decimal.
 * Returns 1 if decimal found, 0 if none.
 */
int is_float(const char *str)
{
	while (*str) {
		if (*str == '.' && str[1] != '\0') {
			return 1;
		}
		str++;
	}
	return 0;
}

/*
 * Takes in a string and parses to make sure all characters are
 * numbers and that they are all in the range of 0-7.
 */
int is_octal(const char *str)
{
	if (*str == '0' && str[1] != '\0') {
		while (*++str) {
			if (*str < '0' || *str > '7') {
				return 0;
			}
		}
		return 1;
	}
	return 0;
}

/*
 * Takes in a string (str) and parses to make sure all characters are
 * numbers and or letters that are all in the hex character range (0-9, a - f)
 */
int is_hex(const char *str)
{
	char curr_char;

	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
		str += 2;
		while ((curr_char = *str++)) {
			if (!isxdigit(curr_char)) {
				return 0;
			}
		}
		return 1;
	}
	return 0;
}

/* Prints token type and the token to the user */
void print_token(const char *type, const char *token)
{
	if (type == NULL)
		die("Error on finding type for %s", token);
	printf("%s: \"%s\"\n", type, token);
}

/* Frees up memory being used by input token list */
void free_list(struct input_token *list)
{
	struct input_token *temp;
	while (list) {
		temp = list;
		list = list->next;
		free(temp->input);
		free(temp);
	}
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
		if (!isspace(*parse)) {
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
struct input_token *split_tokens(char *arg)
{
	struct input_token *head = NULL, **list_walker = NULL;
	int strlen;
	char *start_of_token, *parser = arg;

	list_walker = &head;
	while (1) {
		/* Skip all white space */
		while (isspace(*parser))
			parser++;

		if (*parser == '\0')
			break;

		/* If we find a single-line comment, skip until we find a \n */
		if (*parser == '/' && parser[1] == '/'){
			while (*parser != '\n' && *parser != '\0'){
				parser++;
			}
			continue;
		}

		/* If we find a multi-line comment, skip until we find a * / 
			THIS SHOULD BE WORKING BUT ITS NOT???*/
		if (*parser == '/' && parser[1] == '*'){
			parser += 2; //skip the * to get to next symbol
			while (*parser != '*' && parser[1] != '/' && *parser != '\0'){
				parser++;
			}
			if (*parser != '\0'){
				parser += 2; //skip the / to get to next symbol
			}
			continue;
		}

		/* Track the beginning of token and it's length */
		start_of_token = parser;
		strlen = 0;
		while (!isspace(*parser) && *parser != '\0') {
			parser++;
			strlen++;
		}
		/* Allocate space and then copy the token to be used later */
		*list_walker = malloc(sizeof(**list_walker));
		if (!*list_walker)
			die("Error allocating space to list_walker");
		(*list_walker)->input = malloc(sizeof(char) * (strlen + 1));
		if (!(*list_walker)->input)
			die("Error allocating space to list_walker->input");
		strcopy(start_of_token, (*list_walker)->input, strlen);
		list_walker = &(*list_walker)->next;
	}
	return head;
}

/*
 * Takes in a pointer to an input token list and goes throught the list.
 * At every point in the list it checks what type of token it is and then
 * prints that type and token to the user. This is called after all tokens
 * have been read from command line AND have been sanitized.
 */
void parse_tokens(struct input_token *list)
{
	const char *token, *token_type;
	int i;

	while (list) {
		token = list->input;
		token_type = NULL;

		/* If it starts with a letter its prob a word */
		if (isalpha(*token)) {
			for (i = 0; i < ARRAY_SIZE(C_keywords); i++){
				if (!strcmp(token, C_keywords[i].operator)) {
					token_type = C_keywords[i].name;
					break;
				}
			}

			if (token_type == NULL)
				token_type = "word";
		/* If the starting char is a number it has to be a number */
		} else if (isdigit(*token)) {
			if (is_hex(token)) {
				token_type = "hex integer";
			} else if (is_octal(token)) {
				token_type = "octal integer";
			} else if (is_float(token)) {
				token_type = "float";
			} else {
				token_type = "decimal integer";
			}
		/* If it is neither a number nor letter then it must be a symbol */
		} else {
			for (i = 0; i < ARRAY_SIZE(C_tokens); i++) {
				if (!strcmp(token, C_tokens[i].operator)) {
					token_type = C_tokens[i].name;
					break;
				}
			}
		}

		print_token(token_type, token);
		list = list->next;
	}
}

/*
 * Splits a input token into two seperate token nodes.
 * Where the input string is split is determined by toklen.
 * This function is mainly going to be called for sanitizing
 * input tokens that have multiple tokens attached together
 * such as array[123] or 123abc.
 * Ex: OriginalNode -> "123abc"
 *     |
 *     |- node1 -> "123"
 *     |- node2 -> "abc"
 */
void split_token(struct input_token **token_node, int toklen)
{
	struct input_token *first, *second;
	int remaining_length;

	first = malloc(sizeof(*first));
	second = malloc(sizeof(*second));

	if (!first || !second)
		die("Error allocating space in split_token()");

	first->input = malloc(sizeof(char) * (toklen + 1));
	if (!first->input)
		die("Error allocating space to first->input");
	strcopy((*token_node)->input, first->input, toklen);
	first->next = second;

	remaining_length = strlen((*token_node)->input) - toklen;

	second->input = malloc(sizeof(char) * (remaining_length + 1));
	if (!second->input)
		die("Error allocating space to second->input");
	strcopy(&((*token_node)->input[toklen]), second->input, remaining_length);
	second->next = (*token_node)->next;

	/* Free the old node we don't need anymore */
	free((*token_node)->input);
	free(*token_node);
	*token_node = first;
}

/*
 * Sanitizes strings starting with a letter. Splits token
 * in two if it runs into a character that is NOT alphanumeric.
 */
void sanitize_word(struct input_token **token_node)
{
	char *parser = (*token_node)->input;
	int toklen = 0;

	while (*parser) {
		if (!isalnum(*parser)) {
			split_token(token_node, toklen);
			break;
		}
		toklen++;
		parser++;
	}
}

/*
 * This needs to be rewritten.
 * This will be split into different functions to
 * sanitize floats, octal, and decimal.
 */
void sanitize_num(struct input_token **token_node)
{
	char *parser = (*token_node)->input;
	int toklen = 0, hex_num = 0, float_num = 0;
	if (*parser == '0') {
		if (parser[1] == 'x' || parser[1] == 'X') {
			hex_num = 1;
			parser += 2;
			toklen += 2;
		}
	}

	while (*parser) {
		if ((isalpha(*parser) && !hex_num) || (isalpha(*parser) && !isxdigit(*parser))) {
			if (float_num == 1 && *parser == 'e') {
				parser++;
				toklen++;
			} else {
				split_token(token_node, toklen);
				break;
			}
		} else if (ispunct(*parser)) {
			if (*parser == '.' && !hex_num) {
				float_num = 1;
			} else {
				split_token(token_node, toklen);
				break;
			}
		}
		toklen++;
		parser++;
	}
}

/*
 * Sanitizes strings starting with a symbol. Given what the first symbol is
 * it is easy to discern what should follow. If the symbol does not make up
 * the full length of the token, split it in two.
 */
void sanitize_symbol(struct input_token **token_node)
{
	char *parser = (*token_node)->input;
	int toklen = 0, full_token_length;

	full_token_length = strlen(parser);
	switch (*parser) {
	/* The following symbols can ONLY be themselves */
	case '(':
	case ')':
	case '[':
	case ']':
	case '.':
	case ',':
	case '!':
	case '~':
	case '^':
	case '?':
	case ':':
		toklen = 1;
		break;

	/*
	 * The following symbols can ONLY be themselves: '=', '*', etc.
	 * or follwing an '=': '==', '*=', '\=', etc.
	 */
	case '=':
	case '*':
	case '%':
	case '/':
		toklen = (parser[1] == '=') ? 2 : 1;
		break;

	/*
	 * '-' is a special case such that it can be alone '-' or following
	 * '>', '-', or '='
	 */
	case '-':
		toklen = (parser[1] == '>' || parser[1] == '-' || parser[1] == '=') ? 2 : 1;
		break;
	/*
	 * -, |, +, & can can either be themselves: '+', '|', etc.
	 *  following themselves: '++', '||', etc.
	 *  or following an equals: '-=', '|=', '+=', '&='
	 */
	case '|':
	case '+':
	case '&':
		toklen = (parser[1] == *parser || parser[1] == '=') ? 2 : 1;
		break;

	/*
	 * > and < can be either themselves: '>' '<'
	 * following themselves: '<<' '>>'
	 * following an equals: '>=' '<='
	 * or following themselves with an equals: '<<=' '>>='
	 */
	case '>':
	case '<':
		if (parser[1] == *parser)
			toklen = (parser[2] == '=') ? 3 : 2;
		else if (parser[1] == '=')
			toklen = 2;
		else
			toklen = 1;
		break;

	case '0':
	default:
		break;
	}
	/* If our symbol doesnt take up the full length of the string. Then split it */
	if (full_token_length != toklen)
		split_token(token_node, toklen);
}

/*
 * Kick off function to start sanizing each token within
 * the input token list. Simply looks at the first character
 * of each input string and the sanitize operation is decided
 * on what type of character that is.
 */
void sanitize_tokens(struct input_token **list)
{
	const char *token;
	while (*list) {
		token = (*list)->input;
		if (isalpha(*token)) {
			/* Sanitize word */
			sanitize_word(list);
		} else if (isdigit(*token)) {
			/* sanitize number */
			sanitize_num(list);
		} else {
			/* sanitize symbol */
			sanitize_symbol(list);
		}
		list = &(*list)->next;
	}
}

int main(int argc, char **argv)
{
	struct input_token *head;
#if DEBUG
	struct input_token *parser;
#endif

	if (argc < 2)
		die("Please include a string.");
	if (argc > 2)
		die("Received too many inputs.");
	if (num_of_tokens(argv[1]) == 0)
		return 0;
	/* Parse command line args */


	head = split_tokens(argv[1]);
#if DEBUG
	printf("===After input===\n");
	for (parser = head; parser != NULL; parser = parser->next)
		printf("%s\n", parser->input);
	sanitize_tokens(&head);
	printf("===After sanitize===\n");
	for (parser = head; parser != NULL; parser = parser->next)
		printf("%s\n", parser->input);
#else

	sanitize_tokens(&head);
#endif
	parse_tokens(head);
	free_list(head);

	return 0;
}
