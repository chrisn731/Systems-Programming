CC 	:= gcc
CFLAGS += -std=c99 -Wpedantic -pedantic-errors
CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -Waggregate-return
CFLAGS += -Wbad-function-cast
CFLAGS += -Wcast-align
CFLAGS += -Wcast-qual
CFLAGS += -Wdeclaration-after-statement
CFLAGS += -Wformat=2
CFLAGS += -Wlogical-op
CFLAGS += -Wmissing-declarations
CFLAGS += -Wmissing-include-dirs
CFLAGS += -Wmissing-prototypes
CFLAGS += -Wnested-externs
CFLAGS += -Wpointer-arith
CFLAGS += -Wredundant-decls
CFLAGS += -Wsequence-point
CFLAGS += -Wshadow
CFLAGS += -Wstrict-prototypes
CFLAGS += -Wswitch
CFLAGS += -Wundef
CFLAGS += -Wunreachable-code
CFLAGS += -Wunused-but-set-parameter
CFLAGS += -Wwrite-strings
CFLAGS += -pthread # access to pthread lib
CFLAGS += -D_DEFAULT_SOURCE # access to DT_DIR and DT_REG

CSRC   := Asst2.c filehandler.c dirhandler.c \
	  data.c

EXE := detector

all: $(EXE)

$(EXE): $(CSRC)
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -f detector
