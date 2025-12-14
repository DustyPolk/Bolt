# ⚡ BOLT - High-Performance HTTP Server
# Windows build with MinGW/w64devkit

CC = gcc
CFLAGS = -Wall -Wextra -O3 -march=native -I./include
LDFLAGS = -lws2_32 -lmswsock

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/bolt_server.c \
       $(SRC_DIR)/iocp.c \
       $(SRC_DIR)/threadpool.c \
       $(SRC_DIR)/connection.c \
       $(SRC_DIR)/memory_pool.c \
       $(SRC_DIR)/file_cache.c \
       $(SRC_DIR)/file_sender.c \
       $(SRC_DIR)/http.c \
       $(SRC_DIR)/file_server.c \
       $(SRC_DIR)/mime.c \
       $(SRC_DIR)/utils.c

# Object files
OBJS = $(OBJ_DIR)/main.o \
       $(OBJ_DIR)/bolt_server.o \
       $(OBJ_DIR)/iocp.o \
       $(OBJ_DIR)/threadpool.o \
       $(OBJ_DIR)/connection.o \
       $(OBJ_DIR)/memory_pool.o \
       $(OBJ_DIR)/file_cache.o \
       $(OBJ_DIR)/file_sender.o \
       $(OBJ_DIR)/http.o \
       $(OBJ_DIR)/file_server.o \
       $(OBJ_DIR)/mime.o \
       $(OBJ_DIR)/utils.o

# Target executable
TARGET = bolt.exe

# Default target
all: $(OBJ_DIR) $(TARGET)

# Create obj directory
$(OBJ_DIR):
	-mkdir $(OBJ_DIR) 2>nul || true

# Link
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compile source files
$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/bolt_server.o: $(SRC_DIR)/bolt_server.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/iocp.o: $(SRC_DIR)/iocp.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/threadpool.o: $(SRC_DIR)/threadpool.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/connection.o: $(SRC_DIR)/connection.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/memory_pool.o: $(SRC_DIR)/memory_pool.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/file_cache.o: $(SRC_DIR)/file_cache.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/file_sender.o: $(SRC_DIR)/file_sender.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/http.o: $(SRC_DIR)/http.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/file_server.o: $(SRC_DIR)/file_server.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/mime.o: $(SRC_DIR)/mime.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/utils.o: $(SRC_DIR)/utils.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run the server
run: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	-rm -rf $(OBJ_DIR) $(TARGET) 2>nul || del /q $(OBJ_DIR)\*.o $(TARGET) 2>nul || true

# Rebuild from scratch
rebuild: clean all

# Debug build
debug: CFLAGS += -g -DBOLT_DEBUG -O0
debug: clean all

# Release build with maximum optimization
release: CFLAGS += -O3 -DNDEBUG -flto
release: LDFLAGS += -flto
release: clean all

.PHONY: all run clean rebuild debug release
