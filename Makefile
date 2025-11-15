# Makefile for FAT32 project

CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
TARGET = fat32_test

SRCS = fat32.c disk_io.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c include/*.h 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
