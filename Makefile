
CC = gcc
CFLAGS = -Os -Wall -lm 

all: hello01 hello02 hello03 hello04 hello05 hello06

hello01: hello01.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

hello02: hello02.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

hello03: hello03.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

hello04: hello04.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

hello05: hello05.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

hello06: hello06.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

hello07: hello07.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

lua/liblua.a:
	make -C lua generic
 
