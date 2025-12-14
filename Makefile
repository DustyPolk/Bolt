# ⚡ BOLT - High-Performance HTTP Server
# Windows build with MinGW/w64devkit

CC = gcc
CFLAGS = -Wall -Wextra -O3 -march=native -I./include
# zlib is optional - compression will gracefully degrade if not available
# To enable compression, install zlib and uncomment -lz below
LDFLAGS = -lws2_32 -lmswsock -lsecur32 -lcrypt32 -ladvapi32 -lpsapi
# LDFLAGS += -lz  # Uncomment if zlib is installed

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
       $(SRC_DIR)/utils.c \
       $(SRC_DIR)/compression.c \
       $(SRC_DIR)/config.c \
       $(SRC_DIR)/logger.c \
       $(SRC_DIR)/metrics.c \
       $(SRC_DIR)/vhost.c \
       $(SRC_DIR)/tls.c \
       $(SRC_DIR)/rewrite.c \
       $(SRC_DIR)/proxy.c \
       $(SRC_DIR)/http2.c \
       $(SRC_DIR)/service.c \
       $(SRC_DIR)/reload.c \
       $(SRC_DIR)/master.c \
       $(SRC_DIR)/profiler.c

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
       $(OBJ_DIR)/utils.o \
       $(OBJ_DIR)/compression.o \
       $(OBJ_DIR)/config.o \
       $(OBJ_DIR)/logger.o \
       $(OBJ_DIR)/metrics.o \
       $(OBJ_DIR)/vhost.o \
       $(OBJ_DIR)/tls.o \
       $(OBJ_DIR)/rewrite.o \
       $(OBJ_DIR)/proxy.o \
       $(OBJ_DIR)/http2.o \
       $(OBJ_DIR)/service.o \
       $(OBJ_DIR)/reload.o \
       $(OBJ_DIR)/master.o \
       $(OBJ_DIR)/profiler.o

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

$(OBJ_DIR)/compression.o: $(SRC_DIR)/compression.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/config.o: $(SRC_DIR)/config.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/logger.o: $(SRC_DIR)/logger.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/metrics.o: $(SRC_DIR)/metrics.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/vhost.o: $(SRC_DIR)/vhost.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tls.o: $(SRC_DIR)/tls.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/rewrite.o: $(SRC_DIR)/rewrite.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/proxy.o: $(SRC_DIR)/proxy.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/http2.o: $(SRC_DIR)/http2.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/service.o: $(SRC_DIR)/service.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/reload.o: $(SRC_DIR)/reload.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/master.o: $(SRC_DIR)/master.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/profiler.o: $(SRC_DIR)/profiler.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run the server
run: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	-rm -rf $(OBJ_DIR) $(TARGET) test_runner.exe 2>nul || del /q $(OBJ_DIR)\*.o $(TARGET) test_runner.exe 2>nul || true

# Rebuild from scratch
rebuild: clean all

# Debug build
debug: CFLAGS += -g -DBOLT_DEBUG -O0
debug: clean all

# Release build with maximum optimization
release: CFLAGS += -O3 -DNDEBUG -flto
release: LDFLAGS += -flto
release: clean all

#============================================================================
# Test Suite
#============================================================================

# Test directories
TEST_DIR = tests
TEST_OBJ_DIR = $(OBJ_DIR)/tests

# Test source files
TEST_SRCS = $(TEST_DIR)/test_main.c \
            $(TEST_DIR)/test_utils.c \
            $(TEST_DIR)/test_http.c \
            $(TEST_DIR)/test_mime.c \
            $(TEST_DIR)/test_rewrite.c \
            $(TEST_DIR)/test_config.c \
            $(TEST_DIR)/test_pool.c \
            $(TEST_DIR)/test_cache.c \
            $(TEST_DIR)/test_server.c

# Library objects (exclude main.o since tests have their own main)
LIB_OBJS = $(OBJ_DIR)/bolt_server.o \
           $(OBJ_DIR)/iocp.o \
           $(OBJ_DIR)/threadpool.o \
           $(OBJ_DIR)/connection.o \
           $(OBJ_DIR)/memory_pool.o \
           $(OBJ_DIR)/file_cache.o \
           $(OBJ_DIR)/file_sender.o \
           $(OBJ_DIR)/http.o \
           $(OBJ_DIR)/file_server.o \
           $(OBJ_DIR)/mime.o \
           $(OBJ_DIR)/utils.o \
           $(OBJ_DIR)/compression.o \
           $(OBJ_DIR)/config.o \
           $(OBJ_DIR)/logger.o \
           $(OBJ_DIR)/metrics.o \
           $(OBJ_DIR)/vhost.o \
           $(OBJ_DIR)/tls.o \
           $(OBJ_DIR)/rewrite.o \
           $(OBJ_DIR)/proxy.o \
           $(OBJ_DIR)/http2.o \
           $(OBJ_DIR)/service.o \
           $(OBJ_DIR)/reload.o \
           $(OBJ_DIR)/master.o \
           $(OBJ_DIR)/profiler.o

# Test executable
TEST_TARGET = test_runner.exe

# Build and run tests
test: $(LIB_OBJS) $(TEST_TARGET)
	./$(TEST_TARGET)

# Build test runner
$(TEST_TARGET): $(LIB_OBJS) $(TEST_SRCS)
	$(CC) $(CFLAGS) -I./tests $(TEST_SRCS) $(LIB_OBJS) -o $(TEST_TARGET) $(LDFLAGS)

# Run unit tests only (no integration tests that need server running)
test-unit: $(LIB_OBJS) $(TEST_TARGET)
	@echo "Running unit tests (integration tests will be skipped if server not running)"
	./$(TEST_TARGET)

.PHONY: all run clean rebuild debug release test test-unit
