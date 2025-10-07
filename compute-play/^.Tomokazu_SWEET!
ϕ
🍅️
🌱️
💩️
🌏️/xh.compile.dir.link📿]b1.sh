#!/bin/bash

# Initialize variables
EXE_NAME="emoji_paint"
LIBRARIES="-lGL -lglut -lX11 -lfreetype"
CFLAGS="-c -I/usr/include/freetype2"
LDFLAGS=""
OBJECTS=""

# Remove any existing object files
rm -f *.o

# Check if any .c files exist
if ! ls *.c >/dev/null 2>&1; then
    echo "Error: No .c files found in the current directory"
    exit 1
fi

# Compile each .c file to an object file
echo "Compiling source files..."
for source in *.c; do
    object="${source%.c}.o"
    OBJECTS="$OBJECTS $object"
    echo "  Compiling $source -> $object"
    gcc $CFLAGS "$source" -o "$object"
    if [ $? -ne 0 ]; then
        echo "Error: Compilation of $source failed"
        exit 1
    fi
done

# Link object files to create executable
echo "Linking object files to create $EXE_NAME..."
gcc $OBJECTS -o "$EXE_NAME" $LIBRARIES $LDFLAGS
if [ $? -ne 0 ]; then
    echo "Error: Linking failed"
    exit 1
fi

# Make the executable file executable
echo "Setting execute permissions on $EXE_NAME"
chmod +x "$EXE_NAME"
if [ $? -ne 0 ]; then
    echo "Error: Failed to set execute permissions"
    exit 1
fi

# Clean up object files
echo "Cleaning up object files..."
rm -f $OBJECTS

echo "Compilation successful! Executable created: $EXE_NAME"
