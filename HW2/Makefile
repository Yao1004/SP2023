CC=gcc
CFLAGS=-Wall
DBGFLAGS=-g -fsanitize=address
main:
	$(CC) $(CFLAGS) service.c -o service
debug:
	$(CC) $(CFLAGS) $(DBGFLAGS) service.c -o service
clean:
	rm -f service
