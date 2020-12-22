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
### MyMalloc
An implementation of malloc that detects common programming/usage errors.
The interface for mymalloc is the similar as malloc, but has parameters for filenames and line numbers
for easier debugging:<br/>
`void *mymalloc(size_t x, const char *filename, const int line_number)`<br/>
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

### MyFree
Similarly, to free blocks given by mymalloc use:<br/>
`void myfree(void *ptr, const char *filename, const int line_number)`<br/>
The advantage to using this is that it catches common mistakes such as: redunant freeing of
pointers, attempting to free NULL pointers, or attempting to free pointers not given by mymalloc.

### Memgrind
Asst1 also includes `memgrind.c` that goes through multiple rigorous tests to ensure that mymalloc works through
different types of workload stress.

## Asst2 - File Analysis
A file analyzer that uses threading and Jensen-Shannon Distance computation to calculate
the similarities between two files. The file analyzer takes in a directory and will parse it of all
files and directories. Any directory found will also be checked recursively. All files found within all
found directories will be compared together. Therefore given n files, there will be (1/2)(n)(n-1) or
(n CHOOSE 2) comparisons.<br/>
Sample Output:
```
./detector testdirectory
0.100000 "./testdirectory/test1.txt" and "./testdirectory/test2.txt"
0.150515 "./testdirectory/test1.txt" and "./testdirectory/subdir/test3.txt"
0.225234 "./testdirectory/test2.txt" and "./testdirectory/subdir/test3.txt"
```

## Asst3 - TBD
A server using C sockets to respond to clients with knock knock knock jokes.
```
    Server		    		    Client
==============				==============
> Knock, knock.
					Who's there? <
> Who.

					  Who, who?  <
> I didn't know you were
	an owl!
				      Terrible joke! <
> [CONNECTION CLOSED]
				 [CONNECTION CLOSED] <
```
### KKJ Message System
In order to achieve consistent message sending and make sure neither the client or server
are sending broken messages, Asst3 uses the KKJ message system. The client and server send
messages to each other that denote message type, length, and the payload contents.<br/>
For example, `REG|12|Knock, knock.|` and `REG|11|Who's there?|`.<br/>
When an error is encountered, an error message like `ERR|M1CT|` is sent to the host that
sent the faulty message. After the message is sent, connection is closed to the remote host.
