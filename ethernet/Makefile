
CFLAGS=-Wall -Wextra -g -D_GNU_SOURCE
LDFLAGS=
CC=gcc
OBJS=main.o device.o ethernet.o
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


