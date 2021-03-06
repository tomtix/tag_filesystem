TARGET=tagfs
SRC=$(wildcard *.c) $(wildcard ../cutil/*.c)

CFLAGS=-Wall -std=gnu99 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
CFLAGS+=-iquote. $(shell pkg-config --cflags fuse)
CFLAGS+=-I ../include -I ..

ifdef DEBUG
	CFLAGS+=-ggdb -O0 -DDEBUG
else
	CFLAGS+=-g -O2 -march=native
endif

LDFLAGS=$(shell pkg-config --libs fuse)
LDFLAGS+=$(shell pkg-config --libs sqlite3)

OBJ=$(SRC:.c=.o) schema.o
DEP=$(SRC:.c=.d) 

all: $(TARGET)

-include $(DEP)

schema.c: schema.sql
	../scripts/embedder.py schema.sql > $@

$(TARGET): $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o
	@$(CC) -MM $(CFLAGS) $*.c > $*.d

mnt: 
	mkdir -p mnt/

clean: 
	$(RM) $(OBJ) $(DEP) *.log *.o schema.c

start: mnt tagfs
	./tagfs images mnt -f -d -s

restart: stop kill start

debug: tagfs stop
	gdb --args ./tagfs images/ mnt -f -d -s

val3: tagfs stop
	valgrind ./tagfs images mnt -f -d -s

stop:
	fusermount -u mnt || true

kill:
	kill -9 $(shell pidof tagfs) || true

all_in_one: schema.c
	CFLAGS=$(shell pkg-config --cflags sqlite3) LDFLAGS=$(shell pkg-config --libs sqlite3) ../scripts/all_in_one 

