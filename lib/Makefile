
CFLAGS=-Wall -Wextra -g -D_GNU_SOURCE
CC=gcc
OBJS=logging.o ports.o endian.o timer.o
LIB=lib.a

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@touch $(OBJS) $(PROG)
	@rm $(OBJS) $(PROG)

rec: clean all

.PHONY: all rec clean


