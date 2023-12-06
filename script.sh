#!/bin/bash

# C++ source file
cpp_file="BSkipList.cpp" 

# Output executable file
output_file="BSkipList"

# Compile the C++ program
make "$output_file"

# Check if compilation was successful
if [ $? -eq 0 ]; then
  echo "Compilation successful. Running the program..."
  # Run the compiled program
  "./$output_file" 1000000 1000000 "./test_data_TR/readers_0.txt" "./test_data_TR/writers_0.txt"
else
  echo "Compilation failed."
fi