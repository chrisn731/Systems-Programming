# Systems-Programming
CS 214 - Systems Programming : A story of Chris Naporlee and Michael Nelli take on Zednenem.
Each assignment directory contains a `README.pdf` that contains useful in-depth descriptions and design
explanations of each project.

## Asst0 - String Tokenizer
A string tokenizer written in C that takes in a string as a command line argument and parses it for words,
numbers, and symbols. For example:
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
The interface for mymalloc (which is included in `mymalloc.h` is the same as malloc,
but has parameters for filenames and line numbers for easier debugging:
`void *mymalloc(size_t x, const char *filename, const int line_number)`<br/>
Asst1 also includes `memgrind.c` that goes through multiple rigorous tests to ensure that mymalloc works through
different types of workload stress.
Mymalloc uses the following model to keep track of each block size:
```
__________________________________________________________________________________
| 	      | 			       | 	     |
| Header Data | User-Mutable space             | Header Data | User-Mutable Space ...
|_____________|________________________________|_____________|____________________
^             ^
| 	      |- pointer returned to user.
|- Header uses 16 bits, 1 bit (0/1) for if block is free, remaining 15 for block size.
```

## Asst2 - TBD
