CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LIBS = -lGL -lGLU -lglut -lfreetype -lm

# Directories
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Source files
MAIN_SOURCE = $(SRCDIR)/main.c

# Build targets
TARGET = $(BINDIR)/mojicraft

all: $(TARGET)

$(TARGET): $(OBJDIR)/main.o
	@mkdir -p $(BINDIR)
	$(CC) $^ -o $@ $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/model/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/view/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/controller/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/utils/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/main.o: $(MAIN_SOURCE)
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all clean