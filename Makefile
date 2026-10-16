# ============================================================================
# LinkMate - Makefile
# ============================================================================
# Builds the linkmate binary and the test harness.  Supports debug and
# release build modes, a check target for dependencies, install/uninstall
# with the usual PREFIX / DESTDIR conventions, and a dist target for
# producing a versioned tarball.
#
# Variables users may override on the command line or from the
# environment:
#
#   CC, CFLAGS, LDFLAGS   compiler and flags
#   MARCH                 target CPU (-march=...)
#   PREFIX, DESTDIR       install locations
#   BUILD_MODE            debug | release
# ============================================================================

SHELL = /bin/bash
.SHELLFLAGS = -o pipefail -c

# ----------------------------------------------------------------------------
# Project
# ----------------------------------------------------------------------------
VERSION   = 0.2.0
TARGET    = linkmate
DIST_NAME = $(TARGET)-$(VERSION)
DIST_FILE = $(DIST_NAME).tar.gz

# ----------------------------------------------------------------------------
# Paths
# ----------------------------------------------------------------------------
SRC_DIR   = src
INC_DIR   = include
BUILD_DIR = build
TEST_DIR  = test/unit
MAN_DIR   = man/man1

# ----------------------------------------------------------------------------
# Toolchain
# ----------------------------------------------------------------------------
CC ?= cc
LD  = $(CC)

# ----------------------------------------------------------------------------
# Install paths
# ----------------------------------------------------------------------------
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
MANDIR  ?= $(PREFIX)/share/man
DESTDIR ?=

# ----------------------------------------------------------------------------
# Flags
# ----------------------------------------------------------------------------
MARCH ?= x86-64-v3

# Base flags: only set if the user hasn't already.  A caller running
#   make CFLAGS="-O0 -g"
# keeps full control of optimization; the mandatory warnings below still
# get appended.
CFLAGS  ?= -O2
LDFLAGS ?=

# Mandatory warnings and language settings.  These are not optional and
# are appended even when CFLAGS was set on the command line.
override CFLAGS += -Wall -Wextra -Wpedantic -Wshadow \
                   -Wformat=2 -Wconversion -Wsign-conversion \
                   -std=c99 -D_GNU_SOURCE \
                   -pipe -fno-common \
                   -march=$(MARCH) \
                   -MMD -MP

CFLAGS_DBG  = -g3 -gdwarf-4 -O0 -DDEBUG \
              -fno-omit-frame-pointer \
              -fsanitize=address -fsanitize=undefined
LDFLAGS_DBG = -fsanitize=address -fsanitize=undefined

CFLAGS_REL  = -O3 -DNDEBUG -fomit-frame-pointer -flto
LDFLAGS_REL = -flto

# ----------------------------------------------------------------------------
# Build mode
# ----------------------------------------------------------------------------
BUILD_MODE ?= release
ifeq ($(BUILD_MODE),debug)
    CFLAGS       += $(CFLAGS_DBG)
    LDFLAGS      += $(LDFLAGS_DBG)
    BUILD_SUBDIR  = debug
else
    CFLAGS       += $(CFLAGS_REL)
    LDFLAGS      += $(LDFLAGS_REL)
    BUILD_SUBDIR  = release
endif

# ----------------------------------------------------------------------------
# Files
# ----------------------------------------------------------------------------
C_SRCS = $(wildcard $(SRC_DIR)/*.c)
C_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/$(BUILD_SUBDIR)/%.o,$(C_SRCS))
DEPS   = $(C_OBJS:.o=.d)

# Test harness (no linkmate.c - avoids double main)
TEST_RUNNER  = $(TEST_DIR)/runner.c
TEST_SRCS    = $(wildcard $(TEST_DIR)/test_*.c)
TEST_HARNESS = $(TEST_DIR)/runner

# ----------------------------------------------------------------------------
# Colors
# ----------------------------------------------------------------------------
COLOR_RESET  = \033[0m
COLOR_GREEN  = \033[32m
COLOR_YELLOW = \033[33m
COLOR_RED    = \033[31m
COLOR_CYAN   = \033[36m

# ============================================================================
# TARGETS
# ============================================================================

.PHONY: all clean distclean dist debug release \
        test test-raw test-parser test-terminal \
        install uninstall check help

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

$(TARGET): $(C_OBJS)
	@printf "$(COLOR_YELLOW)🔗 Linking $@...$(COLOR_RESET)\n"
	$(LD) $(LDFLAGS) -o $@ $^
	@printf "$(COLOR_GREEN)   ✓ $(TARGET) linked$(COLOR_RESET)\n"

$(BUILD_DIR)/$(BUILD_SUBDIR)/%.o: $(SRC_DIR)/%.c
	@printf "$(COLOR_YELLOW)📝 Compiling $<...$(COLOR_RESET)\n"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@
	@printf "$(COLOR_GREEN)   ✓ $@$(COLOR_RESET)\n"

-include $(DEPS)

debug: BUILD_MODE = debug
debug: clean build-dir all

release: BUILD_MODE = release
release: clean build-dir all

# ----------------------------------------------------------------------------
# Test harness
# ----------------------------------------------------------------------------
$(TEST_HARNESS):
	@printf "$(COLOR_YELLOW)🔗 Building test harness...$(COLOR_RESET)\n"
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ \
		$(TEST_RUNNER) $(TEST_SRCS) \
		src/browser.c \
		src/terminal.c \
		src/url_parser.c
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
	@command -v $(CC) >/dev/null 2>&1 || { printf "$(COLOR_RED)✗ $(CC) not found$(COLOR_RESET)\n"; exit 1; }
	@printf "$(COLOR_GREEN)   ✓ Compiler: $$($(CC) --version | head -1)$(COLOR_RESET)\n"
	@printf "$(COLOR_GREEN)✅ All checks passed$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Install / Uninstall
# ----------------------------------------------------------------------------
install: $(TARGET)
	@printf "$(COLOR_CYAN)📦 Installing LinkMate $(VERSION)...$(COLOR_RESET)\n"
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	@printf "$(COLOR_GREEN)   ✓ $(DESTDIR)$(BINDIR)/$(TARGET)$(COLOR_RESET)\n"
	install -d $(DESTDIR)$(MANDIR)/man1
	install -m 644 $(MAN_DIR)/$(TARGET).1 $(DESTDIR)$(MANDIR)/man1/$(TARGET).1
	@printf "$(COLOR_GREEN)   ✓ $(DESTDIR)$(MANDIR)/man1/$(TARGET).1$(COLOR_RESET)\n"
	@printf "$(COLOR_GREEN)✅ Installed$(COLOR_RESET)\n"

uninstall:
	@printf "$(COLOR_YELLOW)🗑️  Uninstalling LinkMate...$(COLOR_RESET)\n"
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(MANDIR)/man1/$(TARGET).1
	@printf "$(COLOR_GREEN)   ✓ Removed$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Distribution
# ----------------------------------------------------------------------------
dist: clean
	@printf "$(COLOR_CYAN)📦 Building distribution tarball...$(COLOR_RESET)\n"
	@if [ -n "$$(git status --porcelain)" ]; then \
		printf "$(COLOR_RED)✗ Working tree is not clean.$(COLOR_RESET)\n"; \
		printf "  Commit or stash changes before running 'make dist'.\n"; \
		exit 1; \
	fi
	git archive \
		--format=tar.gz \
		--prefix=$(DIST_NAME)/ \
		-o $(DIST_FILE) \
		HEAD
	@printf "$(COLOR_GREEN)   ✓ $(DIST_FILE) ($$(du -h $(DIST_FILE) | cut -f1))$(COLOR_RESET)\n"
	@printf "$(COLOR_GREEN)✅ Distribution tarball created$(COLOR_RESET)\n"

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
	rm -f $(DIST_FILE)
	@printf "$(COLOR_GREEN)   ✓ $(DIST_FILE) removed$(COLOR_RESET)\n"

# ----------------------------------------------------------------------------
# Help
# ----------------------------------------------------------------------------
help:
	@printf "$(COLOR_CYAN)LinkMate - Build System$(COLOR_RESET)\n\n"
	@printf "$(COLOR_YELLOW)Targets:$(COLOR_RESET)\n"
	@printf "  all           Build LinkMate (release mode, default)\n"
	@printf "  debug         Build with debug symbols and sanitizers\n"
	@printf "  release       Build with optimizations\n"
	@printf "  test          Run all tests\n"
	@printf "  test-raw      Run raw mode tests\n"
	@printf "  test-parser   Run parser tests\n"
	@printf "  test-terminal Run terminal function tests\n"
	@printf "  check         Check dependencies\n"
	@printf "  install       Install to \$$(PREFIX)\n"
	@printf "  uninstall     Remove from \$$(PREFIX)\n"
	@printf "  dist          Create distribution tarball\n"
	@printf "  clean         Remove build files\n"
	@printf "  distclean     Remove build files and distribution tarball\n"
	@printf "  help          Show this help\n"
	@printf "\n"
	@printf "$(COLOR_YELLOW)Variables:$(COLOR_RESET)\n"
	@printf "  PREFIX   Install prefix (default: /usr/local)\n"
	@printf "  DESTDIR  Staging root for package builds (default: empty)\n"
	@printf "  MARCH    Target CPU (default: x86-64-v3)\n"
	@printf "  CC       Compiler (default: cc)\n"
	@printf "  CFLAGS   Additional compiler flags\n"
	@printf "\n"
	@printf "$(COLOR_YELLOW)Examples:$(COLOR_RESET)\n"
	@printf "  make                                         # release build\n"
	@printf "  make debug                                   # debug build\n"
	@printf "  make MARCH=native                            # tune for this CPU\n"
	@printf "  make test                                    # run tests\n"
	@printf "  make dist                                    # create tarball\n"
	@printf "  make install                                 # install to /usr/local\n"
	@printf "  make install PREFIX=\$$HOME/.local            # install to home\n"
	@printf "  make install DESTDIR=/tmp/stage PREFIX=/usr  # staged install\n"
	@printf "  make uninstall PREFIX=\$$HOME/.local         # remove from home\n"
