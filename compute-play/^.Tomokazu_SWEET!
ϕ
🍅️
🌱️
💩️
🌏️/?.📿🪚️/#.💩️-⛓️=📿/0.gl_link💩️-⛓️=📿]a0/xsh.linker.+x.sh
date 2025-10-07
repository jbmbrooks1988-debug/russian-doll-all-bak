#!/bin/bash

echo "🗜️  Compiling Tappy Turd: Flappy Poop Adventure 🚽💩"

# Your working compiler command
file="main.c"
output_dir="."
executable_name="tappy_turd_gl"

# Compile with your exact flags
gcc "$file" \
    game_logic.c \
    render.c \
    input.c \
    audio.c \
    util.c \
    -o "$output_dir/$executable_name" \
    -pthread -lm -lssl -lcrypto -lGL -lGLU -lglut -lfreetype -lavcodec -lavformat -lavutil -lswscale -lX11 \
    -I/usr/include/freetype2 -I/usr/include/libpng12 \
    -L/usr/local/lib -lOpenCL

# Check result
if [ $? -eq 0 ]; then
    echo "✅ Success! Run with: ./$executable_name"
else
    echo "❌ Compilation failed!"
fi
