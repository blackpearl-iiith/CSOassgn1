# Compiler and flags
CC = gcc
CFLAGS = -Wall -g  # -Wall enables all warnings, -g adds debug info
LDFLAGS = -lcrypto # Link against the crypto library for MD5

# All source files
DEPS = sham.h
# Shared object files
OBJ = utils.o

# Default target: build both client and server
all: client server

# Rule to build the client executable
client: client.o $(OBJ)
	$(CC) $(CFLAGS) -o client client.o $(OBJ) $(LDFLAGS)

# Rule to build the server executable
server: server.o $(OBJ)
	$(CC) $(CFLAGS) -o server server.o $(OBJ) $(LDFLAGS)

# Generic rule to compile .c files into .o (object) files
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f *.o client server server_log.txt client_log.txt