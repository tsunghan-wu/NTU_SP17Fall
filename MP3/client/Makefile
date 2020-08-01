.PHONY: all clean

CXX = g++
CFLAGS = -Wall -std=c++11

RM = rm
RMFLAGS = -f

TARGET = miner
OBJECT = miner.o
LIBS = -lssl -lcrypto

all: $(TARGET)

$(TARGET): $(OBJECT)
	@echo "Making $@"
	@$(CXX) $(CFLAGS) -o $@ $(OBJECT) $(LIBS)

%.o: %.cpp
	@echo "Making $@"
	@$(CXX) $(CFLAGS) -c $<

clean:
	$(RM) $(RMFLAGS) $(TARGET) $(OBJECT)
