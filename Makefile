
CC = gcc
CFLAGS = -Os -Wall -lm 

all: bin/hello01 bin/hello02 bin/hello03 bin/hello04 bin/hello05 bin/hello06 bin/hello07 bin/hello08

bin/hello01: hello01.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

bin/hello02: hello02.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

bin/hello03: hello03.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

bin/hello04: hello04.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

bin/hello05: hello05.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

bin/hello06: hello06.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

bin/hello07: hello07.c lua/liblua.a
	$(CC) $(CFLAGS) $^ -o $@

bin/hello08: hello08.c stub-lua.c
	$(CC) $(CFLAGS) $^ -o $@

lua/liblua.a:
	make -C lua generic
 
