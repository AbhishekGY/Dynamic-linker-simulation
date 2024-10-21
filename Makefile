# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic

# Find all .cpp files in the current directory
SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)
EXEC = dynamic_linker_sim

# Phony targets
.PHONY: all clean run

# Default target
all: $(EXEC)

# Compile the executable
$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up compiled files
clean:
	rm -f $(OBJ) $(EXEC)

# Run the program
run: $(EXEC)
	./$(EXEC)

# Print the source files being used
print-source:
	@echo "Source files: $(SRC)"
