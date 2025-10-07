
#!/bin/bash
rm -f *.o
gcc -c -I/usr/include/freetype2 -I/usr/include/libpng12 main.c -o main.o
gcc -c -I/usr/include/freetype2 -I/usr/include/libpng12 view_gl.c -o view_gl.o
gcc -c -I/usr/include/freetype2 -I/usr/include/libpng12 model_gl.c -o model_gl.o
gcc -c -I/usr/include/freetype2 -I/usr/include/libpng12 controller_gl.c -o controller_gl.o
gcc main.o view_gl.o model_gl.o controller_gl.o -o candy_rush🚘️ -pthread -lm -lssl -lcrypto -lGL -lGLU -lglut -lfreetype -lavcodec -lavformat -lavutil -lswscale -lX11 -L/usr/local/lib -lOpenCL
rm -f *.o
