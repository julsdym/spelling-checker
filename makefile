CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -pedantic
spell : spell.c
	$(CC) $(CFLAGS) -o spell spell.c

clean:
	rm -f spell
