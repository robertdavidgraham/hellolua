
CC = gcc
CFLAGS = -Os

all: hello01

hello01: lua/liblua.a hello01.c
	$(CC) $(CFLAGS) $^ -o $@

lua/liblua.a:
	make -C lua generic
 
