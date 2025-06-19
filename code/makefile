CC = gcc
CFLAGS = -Wall -Wextra

all: main

main: main.c
	$(CC) $(CFLAGS) -o download main.c

clean:
	rm -f download
	find . -name "*.txt" -type f -delete
