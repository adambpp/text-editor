CC = gcc
CFLAGS = -Wall -std=c99 -pedantic -g

.PHONY: all clean

all: kilo

clean:
	rm -rf kilo

kilo: kilo.c
	$(CC) $(CFLAGS) kilo.c -o kilo
