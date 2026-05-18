# Project Name and Directories
TARGET_NAME = tree
BUILD_DIR = build
BIN_DIR = bin
EXECUTABLE = $(BIN_DIR)/$(TARGET_NAME)

# --- Hardening Toggle ---
HARDENED ?= 0

# Source and Header files
SRCS = src/main.c src/fs.c src/threading.c src/include/set.c
HDRS = src/include/minicli.h src/include/output.h src/include/types.h src/include/set.h src/include/threading.h

# Object files
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/fs.o $(BUILD_DIR)/threading.o $(BUILD_DIR)/set.o

# --- Compiler and OS Detection ---
CC = gcc
IS_GCC := $(shell $(CC) -v 2>&1 | grep -q "gcc" && echo 1 || echo 0)

# Strict compilation flags
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -pedantic \
         -pedantic-errors -Wall -Wextra -Wformat=2 -Wformat-security -Wnull-dereference \
         -Isrc -Isrc/include

ifeq ($(IS_GCC),1)
    GCC_FLAGS = -Wstack-protector -Wtrampolines -Walloca -Wvla \
                -Warray-bounds=2 -Wimplicit-fallthrough=3 -Wshift-overflow=2 -Wcast-qual \
                -Wcast-align=strict -Wconversion -Wsign-conversion -Wlogical-op -Wduplicated-cond \
                -Wduplicated-branches -Wrestrict -Wnested-externs -Winline -Wundef -Wstrict-prototypes \
                -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wshadow -Wwrite-strings \
                -Wfloat-equal -Wpointer-arith -Wbad-function-cast -Wold-style-definition
    CFLAGS += $(GCC_FLAGS)
endif

# Linking flags
HARDENING_C = -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fstack-clash-protection -fcf-protection
HARDENING_L = -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wl,-z,separate-code -pie

ifeq ($(HARDENED),1)
    SELECTED_HARDENING_C = $(HARDENING_C)
    SELECTED_HARDENING_L = $(HARDENING_L)
else
    SELECTED_HARDENING_C =
    SELECTED_HARDENING_L =
endif

ALL_CFLAGS = $(CFLAGS) $(SELECTED_HARDENING_C) -O3 -pthread
LD_FLAGS = $(SELECTED_HARDENING_L)

# Targets
.PHONY: all clean directories format lint

all: directories $(EXECUTABLE)

directories:
	@mkdir -p $(BIN_DIR) $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/set.o: src/include/set.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(EXECUTABLE): $(OBJS)
	$(CC) $(ALL_CFLAGS) $(LD_FLAGS) -o $@ $^ -pthread

clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR)

format:
	@echo "Formatting code..."
	@clang-format -style=file:./.clang-format -i $(SRCS) $(HDRS)
	@mbake format --config ./.bake.toml Makefile

lint:
	@echo "Running analysis..."
	@clang-tidy -checks=-*,bugprone-*,clang-analyzer-*,performance-* $(SRCS) -- $(CFLAGS)
	@mbake validate --config ./.bake.toml Makefile
