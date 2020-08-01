# define variables
CXX = g++
LIBS = -lcrypto -lssl

RM = rm
RMFLAGS = -f

TARGET = loser
OBJECTS = loser.o list_file.o md5.o

# PHONY
.PHONY: all clean
# rules
all: $(TARGET)

$(TARGET): loser.o list_file.o md5.o
	@echo "Making $@"
	@$(CXX) -o $@ $^ $(LIBS)

.SUFFIX: .o .cpp
.cpp.o:
	@echo "Making $@"
	@$(CXX) -c $<

clean:
	$(RM) $(RMFLAGS) $(TARGET) $(OBJECTS)
