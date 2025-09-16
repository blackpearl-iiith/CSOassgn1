# Define the compiler and the flags
CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm

# Specify the directory for our source and include files
IDIR = include
SDIR = src

# List all C source files
_SRCS = main.c prompt.c parser.c intrinsics.c execute.c jobs.c signals.c
SRCS = $(patsubst %,$(SDIR)/%,$(_SRCS))

# Create a list of object files from the source files
_OBJS = $(_SRCS:.c=.o)
OBJS = $(patsubst %,obj/%,$(_OBJS))

# The final executable name
TARGET = shell.out

# Default target: builds the executable
all: $(TARGET)

# Rule to link the object files into the final executable
$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS)

# Rule to compile source files into object files
obj/%.o: $(SDIR)/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -I$(IDIR) -c $< -o $@

# Target to clean up the build files
clean:
	rm -rf obj $(TARGET)