
CC=gcc
CFLAGS=-Wall
LIBS=-lglut -lGL -lGLU

SRCS=main_prototype_0.c view.c controller.c model.c
OBJS=$(SRCS:.c=.o)

TARGET=main_prototype_0
MODULE_TARGET=module/module_example

.PHONY: all clean module

all: $(TARGET) $(MODULE_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

$(MODULE_TARGET): module/module_example.o
	$(CC) $(CFLAGS) -o $(MODULE_TARGET) module/module_example.o

module/module_example.o: module/module_example.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(MODULE_TARGET) module/*.o
