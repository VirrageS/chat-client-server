TARGET: server client

CC		= gcc
CFLAGS	= -Wall -O2 -std=gnu99 -pedantic-errors

server: server.o err.o chat.o
	$(CC) $(CFLAGS) $^ -o $@

client: client.o err.o chat.o
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: clean TARGET
clean:
	rm -f server client *.o *~ *.bak
