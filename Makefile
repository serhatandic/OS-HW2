# Compiler and linker configurations
CXX = g++
CC = gcc
CXXFLAGS = -Wall -std=c++11
CFLAGS = -Wall
LDFLAGS = -lpthread

# Source files
CPP_SOURCE = simulate.cpp
C_SOURCE = WriteOutput.c helper.c
EXECUTABLE = simulate

# Default target
all: $(EXECUTABLE)

# Compile and link the executable in a single step
$(EXECUTABLE): $(CPP_SOURCE) $(C_SOURCE)
	$(CXX) $(CXXFLAGS) $(CPP_SOURCE) $(C_SOURCE) -o $@ $(LDFLAGS)

# Clean target for removing built files
clean:
	rm -f $(EXECUTABLE)

.PHONY: all clean
