.PHONY: all clean

# compiler name
CC=g++
# place options like -O2, -g here
export CFLAGS=-Wall -ldl -std=c++11

all: inf-bonbon-server

inf-bonbon-server: server_main.cpp  process.cpp
	$(CC) -o $@ $(CFLAGS) server_main.cpp process.cpp

clean:
	rm -f inf-bonbon-server

