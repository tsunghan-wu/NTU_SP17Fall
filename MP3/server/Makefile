.PHONY: all clean

CXX = g++
CFLAGS = -Wall

TARGET = boss
OBJS = boss.o
LIBS = -lssl -lcrypto

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS)

.SUFFIXES: .o .cpp
.cpp.o:
	$(CXX) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJS)
