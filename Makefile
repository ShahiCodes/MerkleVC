CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

LIBS = -lssl -lcrypto -lz

SRC = src/main.cpp src/repository.cpp src/utils.cpp src/commit.cpp src/log.cpp src/restore.cpp
TARGET = mvc

# Default rule: just typing 'make' runs this
all: $(TARGET)

# Rule to link the program
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LIBS)

# Rule to clean up (delete the binary)
clean:
	rm -f $(TARGET)