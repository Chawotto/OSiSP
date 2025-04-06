CC = gcc
CFLAGS = -W -Wall -Wextra -std=c11 -pedantic
TARGET = main

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET)

clean:
	rm -f $(TARGET)
