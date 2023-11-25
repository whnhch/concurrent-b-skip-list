#!/bin/bash

# C++ source file
cpp_file="BSkipList.cpp" 

# Output executable file
output_file="test"

# Compile the C++ program
g++ -o "$output_file" "$cpp_file"

# Check if compilation was successful
if [ $? -eq 0 ]; then
  echo "Compilation successful. Running the program..."
  # Run the compiled program
  "./$output_file"
else
  echo "Compilation failed."
fi