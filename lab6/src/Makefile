
CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -O2 -pthread

.PHONY: all clean

all: client server

client: client.c common.c common.h
	$(CC) $(CFLAGS) client.c common.c -o client

server: server.c common.c common.h
	$(CC) $(CFLAGS) server.c common.c -o server

clean:
	rm -f client server
