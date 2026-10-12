# ============================================================================
# LinkMate - Professional Makefile (2026 Edition)
# ============================================================================
# This Makefile is:
# - Clean
# - Working
# - No duplicate object bullshit
# - Test harness builds without linkmate.c (no double main)
# - Placeholder tests commented out until they're ready
# ============================================================================

SHELL = /bin/bash
.SHELLFLAGS = -o pipefail -c

# ----------------------------------------------------------------------------
# Project Configuration
# ----------------------------------------------------------------------------
VERSION = 0.1.0
TARGET = linkmate
PACKAGE = $(TARGET)-$(VERSION)

# ----------------------------------------------------------------------------
# Paths
# ----------------------------------------------------------------------------
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
DIST_DIR = dist
DOCS_DIR = docs
CONFIG_DIR = config

# ----------------------------------------------------------------------------
# Compiler Configuration
# ----------------------------------------------------------------------------
CC = gcc
LD = $(CC)
NASM = nasm

# Detect architecture
ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
    ARCH_FLAGS = -m64
    NASM_FORMAT = elf64
else ifeq ($(ARCH),i686)
    ARCH_FLAGS = -m32
    NASM_FORMAT = elf32
else ifeq ($(ARCH),aarch64)
    ARCH_FLAGS = -m64
    NASM_FORMAT = elf64
endif

# ----------------------------------------------------------------------------
# Compiler Flags
# ----------------------------------------------------------------------------
CFLAGS_BASE = -Wall -Wextra -Wpedantic -Wshadow \
              -Wformat=2 -Wconversion -Wsign-conversion \
              -std=c99 -D_GNU_SOURCE \
              -pipe -fno-common

CFLAGS_DBG = -g3 -gdwarf-4 -O0 -DDEBUG -fno-omit-frame-pointer \
             -fsanitize=address -fsanitize=undefined

CFLAGS_REL = -O3 -DNDEBUG -fomit-frame-pointer -march=native \
             -flto -fuse-linker-plugin

# Feature flags (auto-detected)
HAVE_AVX2 := $(shell grep -q avx2 /proc/cpuinfo 2>/dev/null && echo 1 || echo 0)
HAVE_AVX512 := $(shell grep -q avx512 /proc/cpuinfo 2>/dev/null && echo 1 || echo 0)

ifeq ($(HAVE_AVX2),1)
    CFLAGS_FEAT = -mavx2 -mpclmul -maes
endif
ifeq ($(HAVE_AVX512),1)
    CFLAGS_FEAT += -mavx512f -mavx512bw -mavx512vl
endif

# Assembly Flags
ASMFLAGS_BASE = -f $(NASM_FORMAT) -g -F dwarf
ifeq ($(HAVE_AVX2),1)
    ASMFLAGS_FEAT = -D__AVX2__
endif
ifeq ($(HAVE_AVX512),1)
    ASMFLAGS_FEAT += -D__AVX512__
endif

# Linker Flags
LDFLAGS_BASE =
LDFLAGS_DBG = -fsanitize=address -fsanitize=undefined
LDFLAGS_REL = -flto -fuse-linker-plugin

# ----------------------------------------------------------------------------
# Build Mode
# ----------------------------------------------------------------------------
BUILD_MODE ?= release
ifeq ($(BUILD_MODE),debug)
    CFLAGS = $(CFLAGS_BASE) $(CFLAGS_DBG) $(CFLAGS_FEAT) $(ARCH_FLAGS)
    LDFLAGS = $(LDFLAGS_BASE) $(LDFLAGS_DBG)
    BUILD_SUBDIR = debug
else
    CFLAGS = $(CFLAGS_BASE) $(CFLAGS_REL) $(CFLAGS_FEAT) $(ARCH_FLAGS)
    LDFLAGS = $(LDFLAGS_BASE) $(LDFLAGS_REL)
    BUILD_SUBDIR = release
endif

# ----------------------------------------------------------------------------
# Files
# ----------------------------------------------------------------------------
C_SRCS = $(wildcard $(SRC_DIR)/*.c)
ASM_SRCS = $(wildcard $(SRC_DIR)/*.asm)
C_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/$(BUILD_SUBDIR)/%.o,$(C_SRCS))
ASM_OBJS = $(patsubst $(SRC_DIR)/%.asm,$(BUILD_DIR)/$(BUILD_SUBDIR)/%.o,$(ASM_SRCS))
OBJS = $(C_OBJS) $(ASM_OBJS)
DEPS = $(OBJS:.o=.d)

# ----------------------------------------------------------------------------
# Test Files
# ----------------------------------------------------------------------------
TEST_UNIT_DIR = test/unit
TEST_RUNNER = $(TEST_UNIT_DIR)/runner.c
TEST_SRCS = $(wildcard $(TEST_UNIT_DIR)/test_*.c)
TEST_HARNESS = test/unit/runner

# ----------------------------------------------------------------------------
# Colors
# ----------------------------------------------------------------------------
COLOR_RESET = \033[0m
COLOR_GREEN = \033[32m
COLOR_YELLOW = \033[33m
COLOR_RED = \033[31m
COLOR_CYAN = \033[36m

# ----------------------------------------------------------------------------
# Installation Paths
# ----------------------------------------------------------------------------
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man/man1

# ============================================================================
# TARGETS
# ============================================================================

.PHONY: all clean debug release test test-raw test-parser test-terminal \
        install uninstall help check

# ----------------------------------------------------------------------------
# Build
# ----------------------------------------------------------------------------
all: build-dir $(TARGET)
	@printf "$(COLOR_GREEN)✅ LinkMate $(VERSION) built successfully!$(COLOR_RESET)\n"
	@printf "$(COLOR_CYAN)   Build mode: $(BUILD_MODE)$(COLOR_RESET)\n"
	@printf "$(COLOR_CYAN)   Target:     $(TARGET)$(COLOR_RESET)\n"
	@printf "$(COLOR_CYAN)   Size:       $$(du -h $(TARGET) | cut -f1)$(COLOR_RESET)\n"

build-dir:
	@mkdir -p $(BUILD_DIR)/$(BUILD_SUBDIR)
	@mkdir -p $(DIST_DIR)

$(TARGET): $(OBJS)
	@printf "$(COLOR_YELLOW)🔗 Linking $@...$(COLOR_RESET)\n"
	$(LD) $(LDFLAGS) -o $@ $^
	@printf "$(COLOR_GREEN)   ✓ $(TARGET) linked$(COLOR_RESET)\n"

$(BUILD_DIR)/$(BUILD_SUBDIR)/%.o: $(SRC_DIR)/%.c
	@printf "$(COLOR_YELLOW)📝 Compiling $<...$(COLOR_RESET)\n"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INC_DIR) -MMD -MP -c $< -o $@
	@printf "$(COLOR_GREEN)   ✓ $@$(COLOR_RESET)\n"

$(BUILD_DIR)/$(BUILD_SUBDIR)/%.o: $(SRC_DIR)/%.asm
	@printf "$(COLOR_YELLOW)⚙️  Assembling $<...$(COLOR_RESET)\n"
	@mkdir -p $(dir $@)
	$(NASM) $(ASMFLAGS_BASE) $(ASMFLAGS_FEAT) $< -o $@
	@printf "$(COLOR_GREEN)   ✓ $@$(COLOR_RESET)\n"

-include $(DEPS)

debug: BUILD_MODE = debug
debug: clean build-dir all

release: BUILD_MODE = release
release: clean build-dir all

# ----------------------------------------------------------------------------
# Test Harness (No linkmate.c - avoids double main)
# ----------------------------------------------------------------------------
$(TEST_HARNESS):
	@printf "$(COLOR_YELLOW)🔗 Building test harness...$(COLOR_RESET)\n"
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ \
		$(TEST_RUNNER) $(TEST_SRCS) \
		src/browser.c \
		src/clipboard.c \
		src/config.c \
		src/history.c \
		src/terminal.c \
		src/url_parser.c \
		src/utils.c
	@printf "$(COLOR_GREEN)   ✓ Test harness built$(COLOR_RESET)\n"

test: build-dir $(TEST_HARNESS)
	@printf "$(COLOR_CYAN)🧪 Running all tests...$(COLOR_RESET)\n"
	./$(TEST_HARNESS)

test-raw: build-dir $(TEST_HARNESS)
	@printf "$(COLOR_CYAN)🧪 Running raw mode tests...$(COLOR_RESET)\n"
	./$(TEST_HARNESS) raw-mode

test-parser: build-dir $(TEST_HARNESS)
	@printf "$(COLOR_CYAN)🧪 Running parser tests...$(COLOR_RESET)\n"
	./$(TEST_HARNESS) parser

test-terminal: build-dir $(TEST_HARNESS)
	@printf "$(COLOR_CYAN)🧪 Running terminal function tests...$(COLOR_RESET)\n"
	./$(TEST_HARNESS) terminal

# ----------------------------------------------------------------------------
# Check
# ----------------------------------------------------------------------------
check:
	@printf "$(COLOR_CYAN)🔍 Checking dependencies...$(COLOR_RESET)\n"
	@command -v $(CC) >/dev/null 2>&1 || { printf "$(COLOR_RED)✗ gcc not found$(COLOR_RESET)\n"; exit 1; }
	@command -v $(NASM) >/dev/null 2>&1 || { printf "$(COLOR_RED)✗ nasm not found$(COLOR_RESET)\n"; exit 1; }
	@printf "$(COLOR_GREEN)   ✓ All dependencies found$(COLOR_RESET)\n"
	@grep -q avx2 /proc/cpuinfo 2>/dev/null && printf "   ✓ AVX2 available\n" || printf "   ⚠ AVX2 not available\n"
	@grep -q avx512 /proc/cpuinfo 2>/dev/null && printf "   ✓ AVX-512 available\n" || printf "   ⚠ AVX-512 not available\n"
	@printf "$(COLOR_GREEN)✅ All checks passed$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Install / Uninstall
# ----------------------------------------------------------------------------
install: $(TARGET)
	@printf "$(COLOR_CYAN)📦 Installing LinkMate...$(COLOR_RESET)\n"
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/
	@printf "$(COLOR_GREEN)   ✓ LinkMate installed to $(BINDIR)$(COLOR_RESET)\n"

uninstall:
	@printf "$(COLOR_YELLOW)🗑️  Uninstalling LinkMate...$(COLOR_RESET)\n"
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	@printf "$(COLOR_GREEN)   ✓ LinkMate uninstalled$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Clean
# ----------------------------------------------------------------------------
clean:
	@printf "$(COLOR_YELLOW)🧹 Cleaning build files...$(COLOR_RESET)\n"
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)
	rm -f $(TEST_HARNESS)
	@printf "$(COLOR_GREEN)   ✓ Cleaned$(COLOR_RESET)\n"

distclean: clean
	@printf "$(COLOR_YELLOW)🧹 Cleaning distribution files...$(COLOR_RESET)\n"
	rm -rf $(DIST_DIR)
	@printf "$(COLOR_GREEN)   ✓ Dist cleaned$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Help
# ----------------------------------------------------------------------------
help:
	@printf "$(COLOR_CYAN)LinkMate - Build System$(COLOR_RESET)\n\n"
	@printf "$(COLOR_YELLOW)Targets:$(COLOR_RESET)\n"
	@printf "  all          Build LinkMate (release mode)\n"
	@printf "  debug        Build with debug symbols\n"
	@printf "  release      Build with optimizations (default)\n"
	@printf "  test         Run all tests\n"
	@printf "  test-raw     Run raw mode tests\n"
	@printf "  test-parser  Run parser tests\n"
	@printf "  test-terminal Run terminal function tests\n"
	@printf "  check        Check dependencies and CPU features\n"
	@printf "  install      Install to system\n"
	@printf "  uninstall    Remove from system\n"
	@printf "  clean        Remove build files\n"
	@printf "  distclean    Remove build and distribution files\n"
	@printf "  help         Show this help\n"
	@printf "\n"
	@printf "$(COLOR_YELLOW)Examples:$(COLOR_RESET)\n"
	@printf "  make                   # Build release version\n"
	@printf "  make debug             # Build debug version\n"
	@printf "  make test              # Run all tests\n"
	@printf "  make test-raw          # Run raw mode tests only\n"
	@printf "  make clean && make     # Rebuild from scratch\n"
