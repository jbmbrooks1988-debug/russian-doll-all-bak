CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LIBS = -lGL -lGLU -lglut
TARGET = main_prototype_0
SOURCE = main_prototype_0.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean