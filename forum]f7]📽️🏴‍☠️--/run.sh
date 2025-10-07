#!/bin/bash
# Compile the server
gcc server.c -o server -std=c99 -Wall

# Check if compilation was successful
if [ $? -eq 0 ]; then
  echo "Compilation successful."
  # Run the server
  ./server
else
  echo "Compilation failed."
fi
