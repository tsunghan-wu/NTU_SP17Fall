.PHONY: all clean

# compiler name
CC=g++

# place options like -O2, -g here
CFLAGS=-lssl -lcrypto -Wall

all: miner

miner: miner.cpp miner.h
	$(CC) -o miner $(CFLAGS) miner.cpp 

clean:
	rm -f miner
