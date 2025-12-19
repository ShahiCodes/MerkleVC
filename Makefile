CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

SRC = src/main.cpp src/repository.cpp
TARGET = mvc

# Default rule: just typing 'make' runs this
all: $(TARGET)

# Rule to link the program
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

# Rule to clean up (delete the binary)
clean:
	rm -f $(TARGET)