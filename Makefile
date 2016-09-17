CC=g++
CFLAGS=-std=c++11 -O3

fastcut: csvcut.cc
	$(CC) $(CFLAGS) $< -o $@
