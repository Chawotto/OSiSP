CC = gcc
CFLAGS = -W -Wall -Wextra -std=c11 -pedantic -Wno-unused-parameter -Wno-unused-variable
LDFLAGS = -pthread

all: lab5_1 lab5_2

lab5_1: lab5_1.c
	$(CC) $(CFLAGS) -o lab5_1 lab5_1.c $(LDFLAGS)

lab5_2: lab5_2.c
	$(CC) $(CFLAGS) -o lab5_2 lab5_2.c $(LDFLAGS)

clean:
	rm -f lab5_1 lab5_2

.PHONY: all clean