
CC = gcc
CFLAGS = -Os -Wall

all: hello01 hello02 hello03 hello04 hello05 hello06

hello01: lua/liblua.a hello01.c
	$(CC) $(CFLAGS) $^ -o $@

hello02: lua/liblua.a hello02.c
	$(CC) $(CFLAGS) $^ -o $@

hello03: lua/liblua.a hello03.c
	$(CC) $(CFLAGS) $^ -o $@

hello04: lua/liblua.a hello04.c
	$(CC) $(CFLAGS) $^ -o $@

hello05: lua/liblua.a hello05.c
	$(CC) $(CFLAGS) $^ -o $@

hello06: lua/liblua.a hello06.c
	$(CC) $(CFLAGS) $^ -o $@

lua/liblua.a:
	make -C lua generic
 
