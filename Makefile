# Variable Definitions
CC = gcc
CFLAGS = -I./include -Wall -g -pthread
LDFLAGS = -L./lib -pthread
LIBS = -lm
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
INCLUDE_DIR = include
TESTS_DIR = tests

SOURCES = $(wildcard $(SRC_DIR)/*.c)					# all source files 
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)	# all object files
EXECUTABLES = $(BIN_DIR)/jobCommander $(BIN_DIR)/jobExecutorServer $(BIN_DIR)/progDelay	# all executables in bin

.PHONY: all clean			# name special commands as phony targets

all: $(EXECUTABLES)

# builds the executables 
$(BIN_DIR)/jobCommander: $(BUILD_DIR)/jobCommander.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BIN_DIR)/jobExecutorServer: $(BUILD_DIR)/jobExecutorServer.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BIN_DIR)/progDelay: $(BUILD_DIR)/progDelay.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BUILD_DIR)/*.o $(EXECUTABLES) *.output
