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
 *
 * 	Read README.PDF for more documentation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DEBUG 0
#define ARRAY_SIZE(X) (int)(sizeof(X)/sizeof(*(X)))

/* Input Token struct to be used to make a linked list of tokens */
struct input_token {
	char *input;
	struct input_token *next;
};

/* C Token struct to be used to make arrays of C keywords and symbols */
struct C_token {
	const char *name;
	const char *operator;
};

void die(const char *msg);
void free_list(struct input_token *);
void parse_tokens(struct input_token *);
void print_token(const char *type, const char *tok);
void sanitize_num(struct input_token **);
void sanitize_symbol(struct input_token **);
void sanitize_tokens(struct input_token **);
void sanitize_word(struct input_token **);
void split_token(struct input_token **, int);
void strcopy(const char *src, char *dest, int n);
int is_float (const char *str);
int is_hex(const char *str);
int is_octal(const char *str);
int num_of_tokens(char *arg);
struct input_token *new_token_node(int);
struct input_token *create_token_list(char *arg);

/*
 * Array to keep track of all operators used in the C language.
 * Used mainly to parse symbols given by user input.
 */
const struct C_token C_tokens[] = {
	{"left parenthesis", "("},
	{"right parenthesis", ")"},
	{"left brace", "["},
	{"right brace", "]"},
	{"structure member", "."},
	{"structure pointer", "->"},
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
	{"modulo", "%"},
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
 * Array to keep track of all keywords used in the C language.
 * Used mainly to parse words given by user input to determine
 * if they are a keyword or not.
 */
const struct C_token C_keywords[] = {
	{"if keyword", "if"},
	{"else keyword", "else"},
	{"do keyword", "do"},
	{"while keyword", "while"},
	{"for keyword", "for"},
	{"char keyword", "char"},
	{"int keyword", "int"},
	{"double keyword", "double"},
	{"float keyword", "float"},
	{"long keyword", "long"},
	{"short keyword", "short"},
	{"return keyword", "return"},
	{"break keyword", "break"},
	{"continue keyword", "continue"},
	{"const keyword", "const"},
	{"struct keyword", "struct"},
	{"unsigned keyword", "unsigned"},
	{"signed keyword", "signed"},
	{"switch keyword", "switch"},
	{"void keyword", "void"},
	{"case keyword", "case"},
	{"default keyword", "default"},
	{"register keyword", "register"},
	{"typedef keyword", "typedef"},
	{"enum keyword", "enum"},
	{"goto keyword", "goto"},
	{"static keyword", "static"},
	{"union keyword", "union"},
	{"volatile keyword", "volatile"},
	{"extern keyword", "extern"},
	{"sizeof keyword", "sizeof"},
};

/*
 * Fatal error exit function.
 * Only called if program runs into unrecoverable error.
 * Does not return.
 */
void die(const char *err)
{
	fprintf(stderr, "%s\n", err);
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
 * Takes in a string (str) and parses it for a decimal.
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
 * Takes in a string (str) and parses to make sure all characters are
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
	if (type != NULL)
		printf("%s: \"%s\"\n", type, token);
	else
		printf("Error on finding type for %s\n", token);
}

/*
 * Takes in an integer representing how long the token node's string
 * is. Then mallocs the appropriate space to hold the node and its token.
 */
struct input_token *new_token_node(int toklen)
{
	struct input_token *new;

	new = malloc(sizeof(*new));
	if (!new)
		die("Error allocating memeory to new token node");
	new->input = malloc(sizeof(char) * (toklen + 1));
	if (!new->input)
		die("Error allocating memory to new token node string");
	new->next = NULL;

	return new;
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
 * Takes in a pointer to a string of tokens and breaks them up by
 * returning pointers to null terminated strings of each token.
 */
struct input_token *create_token_list(char *arg)
{
	struct input_token *head = NULL, **list_walker = NULL;
	int toklen;
	char *start_of_token, *parser = arg;

	list_walker = &head;
	while (1) {
		/* Skip all white space */
		while (isspace(*parser))
			parser++;

		if (*parser == '\0')
			break;

		/* Track the beginning of the token and it's length */
		start_of_token = parser;
		toklen = 0;

		/*
		 * Parse through the characters, if we come across comment
		 * sequence '//' or '/ *' then skip ahead
		 */
		while (!isspace(*parser) && *parser != '\0') {
			if (*parser == '/' && parser[1] == '/') {
				while (*parser != '\n' && *parser != '\0') {
					parser ++;
				}
				break;
			} else if (*parser == '/' && parser[1] == '*') {
				parser += 2;
				while (*parser != '\0') {
					if (*parser == '*' && parser[1] == '/') {
						parser += 2;
						break;
					}
					parser++;
				}
				break;
			} else {
				parser++;
				toklen++;
			}
		}

		/*
		 * Allocate space and then copy the token to be used later
		 * only if we counted > 0 characters.
		 */
		if (toklen > 0) {
			*list_walker = new_token_node(toklen);
			strcopy(start_of_token, (*list_walker)->input, toklen);
			list_walker = &(*list_walker)->next;
		}
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
			token_type = "word";
			/* Scan to see if it is a special C keyword */
			for (i = 0; i < ARRAY_SIZE(C_keywords); i++){
				if (!strcmp(token, C_keywords[i].operator)) {
					token_type = C_keywords[i].name;
					break;
				}
			}
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
	struct input_token *node1, *node2;
	char *token_to_split = (*token_node)->input;
	int remaining_length;

	remaining_length = strlen(token_to_split) - toklen;

	node1 = new_token_node(toklen);
	node2 = new_token_node(remaining_length);

	strcopy(token_to_split, node1->input, toklen);
	node1->next = node2;
	strcopy((token_to_split + toklen), node2->input, remaining_length);
	node2->next = (*token_node)->next;

	/* Free the old node and it's token we don't need anymore */
	free(token_to_split);
	free(*token_node);
	*token_node = node1;
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
 * Takes in a pointer to a pointer to an input node that holds a
 * string starting with a number. Sanitize it such that it has no
 * symbols or letters within it.
 */
void sanitize_num(struct input_token **token_node)
{
	char *parser = (*token_node)->input;
	int toklen = 0, hex_num = 0, float_num = 0;
	if (*parser == '0' && (parser[1] == 'x' || parser[1] == 'X')) {
		hex_num = 1;
		parser += 2;
		toklen += 2;
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
	case '(': /* FALLTHROUGH */
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
	case '=': /* FALLTHROUGH */
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
	case '|': /* FALLTHROUGH */
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
	case '>': /* FALLTHROUGH */
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

	if (argc < 2)
		die("Please include a string to tokenize.");
	if (argc > 2)
		die("Received too many inputs. Please input as one string.");


	/* Create token List */
	head = create_token_list(argv[1]);
	if (!head)
		return 1;

	/* Sanitize the input list */
	sanitize_tokens(&head);

	/* Parse Sanitized Tokens */
	parse_tokens(head);
	/* Free memory */
	free_list(head);

	return 0;
}
