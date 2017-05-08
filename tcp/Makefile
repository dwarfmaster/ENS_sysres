
CFLAGS=-Wall -Wextra -g -D_GNU_SOURCE -I../lib
LDFLAGS=
CC=gcc
OBJS=main.o tcp.o ../lib/lib.a
PROG=eth.out

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS)    -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@touch $(OBJS) $(PROG)
	@rm $(OBJS) $(PROG)

rec: clean all

.PHONY: all rec clean


