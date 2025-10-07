
CC=gcc
CFLAGS=-Wall -I/usr/include/freetype2
LIBS=-lglut -lGL -lGLU -lfreetype

SRCS=main_prototype_0.c view.c controller.c model.c
OBJS=$(SRCS:.c=.o)

TARGET=main_prototype_0
MODULE_TARGET=module/module_example
GAME_TARGET=module/game

.PHONY: all clean module

all: $(TARGET) $(MODULE_TARGET) $(GAME_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

$(MODULE_TARGET): module/module_example.o
	$(CC) $(CFLAGS) -o $(MODULE_TARGET) module/module_example.o

$(GAME_TARGET): module/game.o
	$(CC) $(CFLAGS) -o $(GAME_TARGET) module/game.o

module/module_example.o: module/module_example.c
	$(CC) $(CFLAGS) -c $< -o $@

module/game.o: module/game.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(MODULE_TARGET) $(GAME_TARGET) module/*.o
