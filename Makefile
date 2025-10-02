# Compiler and flags
CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lreadline -lhistory -lncurses

# All .c and .o files
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

# Program name
TARGET = herman

# Default target
all: $(TARGET)

# Linking
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

# Compile each .c file to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(TARGET) $(OBJ)

