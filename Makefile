# Compiler and linker configurations
CXX = g++
CXXFLAGS = -Wall -std=c++11
LDFLAGS = -lpthread

# Source file
SOURCE = simulate.cpp
EXECUTABLE = simulate

# Default target
all: $(EXECUTABLE)

# Compile and link the executable in a single step
$(EXECUTABLE): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $@ $(LDFLAGS)

# Clean target for removing built files
clean:
	rm -f $(EXECUTABLE)

.PHONY: all clean
