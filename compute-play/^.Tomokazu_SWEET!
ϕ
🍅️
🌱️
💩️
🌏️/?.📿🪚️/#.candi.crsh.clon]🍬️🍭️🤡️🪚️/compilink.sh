#!/bin/bash
gcc -c home_gl.c -o home_gl.o -I/usr/include/freetype2
gcc -c view_gl.c -o view_gl.o -I/usr/include/freetype2
gcc -c model_gl.c -o model_gl.o -I/usr/include/freetype2
gcc -c controller_gl.c -o controller_gl.o -I/usr/include/freetype2
gcc home_gl.o view_gl.o model_gl.o controller_gl.o -o game -lglut -lGL -lfreetype
if [ -f game ]; then
    echo 'game executable created successfully.'
else
    echo 'Error: game executable not created.'
fi
ls -l
