.PHONY: all clean

# compiler name
CC=g++

# place options like -O2, -g here
export CFLAGS=-lssl -lcrypto -Wall

all: boss

boss: boss.cpp boss.h 
	$(CC) -o boss $(CFLAGS) boss.cpp 

clean:
	rm -f boss
