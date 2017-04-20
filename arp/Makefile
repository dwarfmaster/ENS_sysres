
CFLAGS=-Wall -Wextra -g -D_GNU_SOURCE -I ../lib
LDFLAGS=-lpthread
CC=gcc
OBJS=main.o arp.o
PROG=eth.out

all: $(PROG)

$(PROG): $(OBJS) ../lib/lib.a
	$(CC) $(CFLAGS)    -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@touch $(OBJS) $(PROG)
	@rm $(OBJS) $(PROG)

rec: clean all

.PHONY: all rec clean


