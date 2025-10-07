#!/bin/bash

# Check if gcc is installed
if ! command -v gcc >/dev/null 2>&1; then
    echo "Error: gcc is not installed. Please install gcc to compile C files."
    exit 1
fi

# Check if any .c files exist
found_files=0
for file in *.c; do
    if [ -f "$file" ]; then
        found_files=1
        break
    fi
done

if [ $found_files -eq 0 ]; then
    echo "No .c files found in the current directory."
    exit 0
fi

# Loop through all .c files in the current directory
for file in *.c; do
    # Only process if the file exists
    if [ -f "$file" ]; then
        # Extract the filename without the .c extension
        basename=${file%.c}

        # Compile the .c file into an executable with the same name
        echo "Compiling $file to $basename..."
        gcc "$file" -o "$basename" -pthread -lm -lssl -lcrypto -lGL -lGLU -lglut -lfreetype -lavcodec -lavformat -lavutil -lswscale -lX11 -I/usr/include/freetype2 -I/usr/include/libpng12 -L/usr/local/lib -lOpenCL

        # Check the exit status of gcc
        if [ $? -eq 0 ]; then
            echo "Successfully compiled $file into $basename"
        else
            echo "Error compiling $file"
            exit 1
        fi
    fi
done

echo "Compilation complete."
