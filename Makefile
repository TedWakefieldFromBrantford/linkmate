# ============================================================================
# LinkMate - Professional Makefile
# ============================================================================
# This Makefile follows best practices:
# - Automatic dependency generation
# - Separate build/ and dist/ directories
# - Proper installation paths
# - Cross-platform compatibility
# - Verbose/quiet modes
# - Colorized output
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
TEST_DIR = tests
CONFIG_DIR = config

# ----------------------------------------------------------------------------
# Compiler Configuration
# ----------------------------------------------------------------------------
CC = gcc
LD = $(CC)
AR = ar
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
# Compiler Flags - Progressive optimization
# ----------------------------------------------------------------------------
# Base flags - always on
CFLAGS_BASE = -Wall -Wextra -Wpedantic -Wshadow \
              -Wformat=2 -Wconversion -Wsign-conversion \
              -std=c99 -D_GNU_SOURCE \
              -pipe -fno-common

# Debug flags
CFLAGS_DBG = -g3 -gdwarf-4 -O0 -DDEBUG -fno-omit-frame-pointer \
             -fsanitize=address -fsanitize=undefined

# Release flags
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

# ----------------------------------------------------------------------------
# Assembly Flags
# ----------------------------------------------------------------------------
ASMFLAGS_BASE = -f $(NASM_FORMAT) -g -F dwarf
ASMFLAGS_FEAT = 
ifeq ($(HAVE_AVX2),1)
    ASMFLAGS_FEAT += -D__AVX2__
endif
ifeq ($(HAVE_AVX512),1)
    ASMFLAGS_FEAT += -D__AVX512__
endif

# ----------------------------------------------------------------------------
# Linker Flags
# ----------------------------------------------------------------------------
LDFLAGS_BASE = 
LDFLAGS_DBG = -fsanitize=address -fsanitize=undefined
LDFLAGS_REL = -flto -fuse-linker-plugin

# ----------------------------------------------------------------------------
# Build Mode (default: release)
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
# File Lists
# ----------------------------------------------------------------------------
C_SRCS = $(wildcard $(SRC_DIR)/*.c)
ASM_SRCS = $(wildcard $(SRC_DIR)/*.asm)
C_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/$(BUILD_SUBDIR)/%.o,$(C_SRCS))
ASM_OBJS = $(patsubst $(SRC_DIR)/%.asm,$(BUILD_DIR)/$(BUILD_SUBDIR)/%.o,$(ASM_SRCS))
OBJS = $(C_OBJS) $(ASM_OBJS)
DEPS = $(OBJS:.o=.d)

# ----------------------------------------------------------------------------
# Installation Paths (FHS compliant)
# ----------------------------------------------------------------------------
PREFIX ?= /usr/local
EXEC_PREFIX ?= $(PREFIX)
BINDIR ?= $(EXEC_PREFIX)/bin
DATADIR ?= $(PREFIX)/share
MANDIR ?= $(DATADIR)/man
DOCDIR ?= $(DATADIR)/doc/$(PACKAGE)
SYSCONFDIR ?= /etc

# ----------------------------------------------------------------------------
# Colors for output (because we're professionals)
# ----------------------------------------------------------------------------
COLOR_RESET = \033[0m
COLOR_GREEN = \033[32m
COLOR_YELLOW = \033[33m
COLOR_BLUE = \033[34m
COLOR_RED = \033[31m
COLOR_CYAN = \033[36m

# ----------------------------------------------------------------------------
# Targets
# ----------------------------------------------------------------------------
.PHONY: all clean distclean install uninstall test docs help
.PHONY: build-dir clean-build dist debug release
.PHONY: check check-deps check-cpu

all: build-dir $(TARGET)
	@printf "$(COLOR_GREEN)✅ LinkMate $(VERSION) built successfully!$(COLOR_RESET)\n"
	@printf "$(COLOR_CYAN)   Build mode: $(BUILD_MODE)$(COLOR_RESET)\n"
	@printf "$(COLOR_CYAN)   Target:     $(TARGET)$(COLOR_RESET)\n"
	@printf "$(COLOR_CYAN)   Size:       $$(du -h $(TARGET) | cut -f1)$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Build Directories
# ----------------------------------------------------------------------------
build-dir:
	@mkdir -p $(BUILD_DIR)/$(BUILD_SUBDIR)
	@mkdir -p $(DIST_DIR)

# ----------------------------------------------------------------------------
# Link the executable
# ----------------------------------------------------------------------------
$(TARGET): $(OBJS)
	@printf "$(COLOR_YELLOW)🔗 Linking $@...$(COLOR_RESET)\n"
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	@printf "$(COLOR_GREEN)   ✓ $(TARGET) linked$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Compile C sources with dependency generation
# ----------------------------------------------------------------------------
$(BUILD_DIR)/$(BUILD_SUBDIR)/%.o: $(SRC_DIR)/%.c
	@printf "$(COLOR_YELLOW)📝 Compiling $<...$(COLOR_RESET)\n"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INC_DIR) -MMD -MP -c $< -o $@
	@printf "$(COLOR_GREEN)   ✓ $@$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Assemble NASM sources
# ----------------------------------------------------------------------------
$(BUILD_DIR)/$(BUILD_SUBDIR)/%.o: $(SRC_DIR)/%.asm
	@printf "$(COLOR_YELLOW)⚙️  Assembling $<...$(COLOR_RESET)\n"
	@mkdir -p $(dir $@)
	$(NASM) $(ASMFLAGS_BASE) $(ASMFLAGS_FEAT) $< -o $@
	@printf "$(COLOR_GREEN)   ✓ $@$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Include dependency files
# ----------------------------------------------------------------------------
-include $(DEPS)

# ----------------------------------------------------------------------------
# Build Modes
# ----------------------------------------------------------------------------
debug: BUILD_MODE = debug
debug: clean build-dir all

release: BUILD_MODE = release
release: clean build-dir all

# ----------------------------------------------------------------------------
# Testing
# ----------------------------------------------------------------------------
test: $(TARGET)
	@printf "$(COLOR_CYAN)🧪 Running tests...$(COLOR_RESET)\n"
	@$(TEST_DIR)/run_tests.sh

check: check-deps check-cpu
	@printf "$(COLOR_GREEN)✅ All checks passed$(COLOR_RESET)\n"

check-deps:
	@printf "$(COLOR_CYAN)🔍 Checking dependencies...$(COLOR_RESET)\n"
	@command -v $(CC) >/dev/null 2>&1 || { printf "$(COLOR_RED)✗ gcc not found$(COLOR_RESET)\n"; exit 1; }
	@command -v $(NASM) >/dev/null 2>&1 || { printf "$(COLOR_RED)✗ nasm not found$(COLOR_RESET)\n"; exit 1; }
	@printf "$(COLOR_GREEN)   ✓ All dependencies found$(COLOR_RESET)\n"

check-cpu:
	@printf "$(COLOR_CYAN)🔍 Checking CPU features...$(COLOR_RESET)\n"
	@grep -q avx2 /proc/cpuinfo 2>/dev/null && printf "$(COLOR_GREEN)   ✓ AVX2 available$(COLOR_RESET)\n" || printf "$(COLOR_YELLOW)   ⚠ AVX2 not available$(COLOR_RESET)\n"
	@grep -q avx512 /proc/cpuinfo 2>/dev/null && printf "$(COLOR_GREEN)   ✓ AVX-512 available$(COLOR_RESET)\n" || printf "$(COLOR_YELLOW)   ⚠ AVX-512 not available$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Documentation
# ----------------------------------------------------------------------------
docs:
	@printf "$(COLOR_CYAN)📚 Building documentation...$(COLOR_RESET)\n"
	@cd $(DOCS_DIR) && doxygen Doxyfile 2>/dev/null || printf "$(COLOR_YELLOW)   ⚠ Doxygen not installed$(COLOR_RESET)\n"
	@printf "$(COLOR_GREEN)   ✓ Documentation built$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Installation (FHS compliant)
# ----------------------------------------------------------------------------
install: $(TARGET)
	@printf "$(COLOR_CYAN)📦 Installing LinkMate...$(COLOR_RESET)\n"
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/
	install -d $(DESTDIR)$(MANDIR)/man1
	install -m 644 $(DOCS_DIR)/$(TARGET).1 $(DESTDIR)$(MANDIR)/man1/
	@if [ -d $(CONFIG_DIR) ]; then \
	    install -d $(DESTDIR)$(SYSCONFDIR)/$(TARGET); \
	    cp -r $(CONFIG_DIR)/* $(DESTDIR)$(SYSCONFDIR)/$(TARGET)/; \
	fi
	@printf "$(COLOR_GREEN)   ✓ LinkMate installed to $(BINDIR)$(COLOR_RESET)\n"

uninstall:
	@printf "$(COLOR_YELLOW)🗑️  Uninstalling LinkMate...$(COLOR_RESET)\n"
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(MANDIR)/man1/$(TARGET).1
	rm -rf $(DESTDIR)$(SYSCONFDIR)/$(TARGET)
	@printf "$(COLOR_GREEN)   ✓ LinkMate uninstalled$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Cleanup
# ----------------------------------------------------------------------------
clean:
	@printf "$(COLOR_YELLOW)🧹 Cleaning build files...$(COLOR_RESET)\n"
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)
	@printf "$(COLOR_GREEN)   ✓ Cleaned$(COLOR_RESET)\n"

distclean: clean
	@printf "$(COLOR_YELLOW)🧹 Cleaning distribution files...$(COLOR_RESET)\n"
	rm -rf $(DIST_DIR)
	@printf "$(COLOR_GREEN)   ✓ Dist cleaned$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Distribution
# ----------------------------------------------------------------------------
dist: distclean docs
	@printf "$(COLOR_CYAN)📦 Creating distribution package...$(COLOR_RESET)\n"
	mkdir -p $(DIST_DIR)
	git archive --format=tar --prefix=$(PACKAGE)/ HEAD | gzip > $(DIST_DIR)/$(PACKAGE).tar.gz
	@printf "$(COLOR_GREEN)   ✓ Package created: $(DIST_DIR)/$(PACKAGE).tar.gz$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Help
# ----------------------------------------------------------------------------
help:
	@printf "$(COLOR_CYAN)LinkMate - Build System$(COLOR_RESET)\n"
	@printf "\n"
	@printf "$(COLOR_YELLOW)Targets:$(COLOR_RESET)\n"
	@printf "  all          Build LinkMate (release mode)\n"
	@printf "  debug        Build with debug symbols\n"
	@printf "  release      Build with optimizations (default)\n"
	@printf "  test         Run tests\n"
	@printf "  check        Check dependencies and CPU features\n"
	@printf "  docs         Build documentation\n"
	@printf "  install      Install to system\n"
	@printf "  uninstall    Remove from system\n"
	@printf "  clean        Remove build files\n"
	@printf "  distclean    Remove build and distribution files\n"
	@printf "  dist         Create distribution tarball\n"
	@printf "  help         Show this help\n"
	@printf "\n"
	@printf "$(COLOR_YELLOW)Variables:$(COLOR_RESET)\n"
	@printf "  BUILD_MODE   build mode: release (default) or debug\n"
	@printf "  PREFIX       installation prefix: /usr/local (default)\n"
	@printf "  CC           C compiler: gcc (default)\n"
	@printf "  NASM         NASM assembler: nasm (default)\n"
	@printf "\n"
	@printf "$(COLOR_YELLOW)Examples:$(COLOR_RESET)\n"
	@printf "  make                   # Build release version\n"
	@printf "  make debug             # Build debug version\n"
	@printf "  make test              # Run tests\n"
	@printf "  make install           # Install to /usr/local\n"
	@printf "  make PREFIX=/usr install # Install to /usr\n"
	@printf "  make clean && make     # Rebuild from scratch\n"

# ----------------------------------------------------------------------------
# .PHONY: All targets are phony
# ----------------------------------------------------------------------------
