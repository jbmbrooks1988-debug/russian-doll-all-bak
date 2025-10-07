# Makefile for player program

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
LIBS = -lGL -lGLU -lglut -lX11 -lfreetype -lm -ldl

# Source files
SRCS = player_main.c core.c managers.c objects.c scenes.c sprites.c windows.c plugins.c camera.c
OBJS = $(SRCS:.c=.o)
TARGET = player_main

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(TARGET)

# Install (copy to player directory)
install: $(TARGET)
	cp $(TARGET) ../player/

# Build and install
build: all install

.PHONY: all clean install build