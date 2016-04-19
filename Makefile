TARGET: server client

CC	= cc
CFLAGS	= -Wall -O2 -std=gnu99
LFLAGS	= -Wall

echo-server: server.o err.o
	$(CC) $(LFLAGS) $^ -o $@

echo-client: client.o err.o
	$(CC) $(LFLAGS) $^ -o $@

.PHONY: clean TARGET
clean:
	rm -f server client *.o *~ *.bak
