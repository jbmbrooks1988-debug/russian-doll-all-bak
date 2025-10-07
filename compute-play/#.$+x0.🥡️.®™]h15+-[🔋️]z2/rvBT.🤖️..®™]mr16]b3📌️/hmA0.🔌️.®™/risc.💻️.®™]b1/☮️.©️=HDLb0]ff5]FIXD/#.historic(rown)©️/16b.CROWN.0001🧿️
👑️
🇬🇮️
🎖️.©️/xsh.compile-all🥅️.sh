#!/bin/bash

# Loop through all .c files in the current directory
for file in *.c; do
    # Check if any .c files exist (if no matches, *.c becomes literal)
    if [ ! -f "$file" ]; then
        echo "No .c files found in the current directory."
        exit 1
    fi

    # Extract the filename without the .c extension
    basename=${file%.c}

    # Compile the .c file into an executable with the same name
    gcc "$file" -o "$basename" -lm

    # Check the exit status of gcc
    if [ $? -eq 0 ]; then
        echo "Successfully compiled $file into $basename"
    else
        echo "Error compiling $file"
    fi
done

echo "Compilation complete."
