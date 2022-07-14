SQLITE_INC=
SQLITE_LIB=-lsqlite3

CLIPS_INC=
CLIPS_LIB=-lclips

CFLAGS = $(SQLITE_INC) $(CLIPS_INC) -I. -Os -g

all: example

clean:
	rm -f SQLiteCLIPS.o example

example: example.c SQLiteCLIPS.o
	cc $(CFLAGS) -o example example.c SQLiteCLIPS.o $(CLIPS_LIB) $(SQLITE_LIB)

check: example
	./example
