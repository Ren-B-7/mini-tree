# Project Name and Directories
TARGET_NAME = tree
BUILD_DIR = build
BIN_DIR = bin
EXECUTABLE = $(BIN_DIR)/$(TARGET_NAME)

# --- OS / Platform Detection ---
ifeq ($(OS),Windows_NT)
    PLATFORM   := windows
    EXE_SUFFIX := .exe
    # MinGW ships python3; fall back to plain python if not found
    PYTHON     := $(shell command -v python3 2>/dev/null || command -v python 2>/dev/null || echo python3)
    # Default install location under the user profile (forward-slashes work in MinGW)
    INSTALL_DIR ?= $(USERPROFILE)/.local/bin
else
    UNAME_S := $(shell uname -s 2>/dev/null || echo unknown)
    ifeq ($(UNAME_S),Darwin)
        PLATFORM := darwin
    else
        PLATFORM := linux
    endif
    EXE_SUFFIX :=
    PYTHON     := $(shell command -v python3 2>/dev/null || command -v python 2>/dev/null || echo python3)
    INSTALL_DIR ?= $(HOME)/.local/bin
endif

# --- Hardening Toggle ---
HARDENED ?= 0

# Source and Header files
SRCS = src/main.c src/fs.c src/threading.c src/include/set.c src/cli.c src/output.c
HDRS = src/include/minicli.h src/include/types.h \
       src/include/set.h src/include/threading.h src/include/fs.h src/include/cli.h src/include/output.h

# Object files
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/fs.o $(BUILD_DIR)/threading.o $(BUILD_DIR)/set.o $(BUILD_DIR)/cli.o $(BUILD_DIR)/output.o

# --- Compiler Detection ---
CC = gcc
IS_GCC := $(shell $(CC) -v 2>&1 | grep -q "gcc" && echo 1 || echo 0)

# Platform Flags
ifeq ($(PLATFORM),windows)
    POSIX_FLAGS =
else
    POSIX_FLAGS = -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
endif

# Strict compilation flags
CFLAGS = -std=c99 $(POSIX_FLAGS) -pedantic -pedantic-errors \
         -Wall -Wextra -Wformat=2 -Wformat-security -Wnull-dereference \
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

# Hardening flags
ifeq ($(PLATFORM),linux)
    HARDENING_C = -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fstack-clash-protection -fcf-protection
    HARDENING_L = -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wl,-z,separate-code -pie
else
    HARDENING_C =
    HARDENING_L =
endif

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
.PHONY: all clean directories default format lint check-tools install uninstall check-binaries format-c format-makefile format-ci lint-c lint-makefile

default: directories $(EXECUTABLE)

all: directories check $(EXECUTABLE)

check: check-tools format lint

check-tools:
	@command -v clang-format > /dev/null 2>&1 || \
	    { echo "ERROR: clang-format not found."; exit 1; }
	@command -v clang-tidy > /dev/null 2>&1 || \
	    { echo "ERROR: clang-tidy not found."; exit 1; }
	@command -v mbake > /dev/null 2>&1 || \
	    { echo "ERROR: mbake not found."; exit 1; }

check-binaries:
	@if [ ! -f "$(EXECUTABLE)" ]; then \
		echo "ERROR: Binaries are missing. Run 'make' first."; \
		echo "  Expected: $(EXECUTABLE)"; \
		exit 1; \
	fi

directories:
	@mkdir -p $(BIN_DIR) $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c
	@echo "Compiling $<..."
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/include/%.c
	@echo "Compiling $<..."
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(EXECUTABLE): $(OBJS)
	$(CC) $(ALL_CFLAGS) $(LD_FLAGS) -o $@ $^ -pthread

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)

format: check-tools format-c format-makefile

format-c:
	@echo "Formatting C source code"
	@clang-format -style=file:./.clang-format -i $(SRCS) $(HDRS)

format-makefile:
	@echo "Formatting Makefile"
	@mbake format --config ./.bake.toml Makefile

format-ci: check-tools format-c-ci format-makefile-ci

format-c-ci:
	@echo "Checking C source file formats"
	@clang-format --dry-run -style=file:./.clang-format -Werror $(SRCS) $(HDRS)

format-makefile-ci:
	@echo "Checking Makefile format"
	@mbake format --config ./.bake.toml --check Makefile

lint: check-tools lint-c lint-makefile

lint-c:
	@echo "Running clang-tidy analysis"
	@clang-tidy -checks=-*,bugprone-*,clang-analyzer-*,performance-* \
	$(SRCS) -- $(CFLAGS)

lint-makefile:
	@echo "Running Makefile analysis"
	@mbake validate --config ./.bake.toml Makefile

install: check-binaries
	@mkdir -p $(INSTALL_DIR)
	@install -m 755 $(EXECUTABLE) $(INSTALL_DIR)/$(TARGET_NAME)$(EXE_SUFFIX)
	@echo "Installed $(TARGET_NAME) to $(INSTALL_DIR)/$(TARGET_NAME)$(EXE_SUFFIX)"

uninstall:
	@rm -f $(INSTALL_DIR)/$(TARGET_NAME)$(EXE_SUFFIX)
	@echo "Uninstalled $(TARGET_NAME) from $(INSTALL_DIR)"
