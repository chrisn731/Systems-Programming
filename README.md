# Systems-Programming
CS 214 - Systems Programming : A story of Chris Naporlee and Michael Nelli take on Zednenem.

## Asst0 - String Tokenizer
A string tokenizer written in C that takes in a string as a command line argument and parses it for words,
numbers, and symbols. For example,
```
./tokenizer "array[xyz ] += pi 3.14159e-10"
word: "array"
left bracket: "["
right bracket "]"
plus equals: "+="
word: "pi"
float: "3.14159e-10"
```

## Asst1 - ++Malloc
An implementation of malloc that detects common programming/usage errors.
The interface for mymalloc is the same as malloc, `void *malloc(size_t x)` but this expands to
`void *mymalloc(size_t x, __FILE__, __LINE__)` to give the user precise calls that caused problems
during dynamic memory allocation.
Asst1 also includes memgrind.c that goes through multiple rigorous tests to ensure that mymalloc works
as well, if not better, than original malloc. This includes tests that make sure you dont free addresses
that are not pointers, freeing pointers that were not allocated by malloc, and redundant freeing of pointers.
