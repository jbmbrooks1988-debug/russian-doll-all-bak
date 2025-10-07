CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
LIBS = -lglut -lGLU -lGL

# Define source files
SOURCES = main_prototype_0.c model.c view.c controller.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = csv_ui_framework

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)
	@echo "Build complete: $(TARGET)"

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Special rule for main_prototype_0.o to ensure proper compilation
main_prototype_0.o: main_prototype_0.c
	$(CC) $(CFLAGS) -c main_prototype_0.c -o main_prototype_0.o

model.o: model.c
	$(CC) $(CFLAGS) -c model.c -o model.o

view.o: view.c
	$(CC) $(CFLAGS) -c view.c -o view.o

controller.o: controller.c
	$(CC) $(CFLAGS) -c controller.c -o controller.o

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Rebuild everything
rebuild: clean all

# Run the program
run: $(TARGET)
	./$(TARGET)

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install freeglut3-dev build-essential

.PHONY: all clean rebuild run install-deps