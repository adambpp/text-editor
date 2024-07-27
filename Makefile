CC = gcc
CFLAGS = -Wall -std=c99 -pedantic -g

.PHONY: all clean

all: kilo

clean:
	rm -rf kilo

count: count.c
	$(CC) $(CFLAGS) kilo.c -o kilo
