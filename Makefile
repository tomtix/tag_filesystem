TARGET=tagfs
SRC=$(wildcard *.c) $(wildcard cutil/*.c)
CFLAGS=-Wall -std=gnu99 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE \
	-Wno-unused-label -Wno-unused-function -iquote.

ifdef DEBUG
	CFLAGS+=-ggdb -O0 -DDEBUG
else
	CFLAGS+=-g -O2 -march=native
endif

LDFLAGS= -L. -lfuse

OBJ=$(SRC:.c=.o) 
DEP=$(SRC:.c=.d) 

all: $(TARGET)

-include $(DEP)

$(TARGET): $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c
	gcc -c $(CFLAGS) $*.c -o $*.o
	gcc -MM $(CFLAGS) $*.c > $*.d

mnt: 
	mkdir -p mnt/

clean: 
	$(RM) $(OBJ) $(DEP) *.log *.o
	make -C cutil clean

start: mnt tagfs
	./tagfs . mnt -f -d -s

restart: stop kill start

debug: tagfs stop
	gdb --args ./tagfs . mnt -f -d -s

val3: tagfs stop
	valgrind ./tagfs images mnt -f -d -s

stop:
	fusermount -u mnt || true

kill:
	kill -9 $(shell pidof tagfs) || true
