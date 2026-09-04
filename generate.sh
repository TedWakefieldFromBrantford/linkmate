#!/bin/bash

# Define modules
modules="linkmate url_parser terminal browser clipboard history config"

# Create directories
mkdir -p src include docs tests config

# Generate source files
for f in $modules; do
    cat > src/$f.c << EOM
#include "../include/$f.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TODO: Implement $f functions */

EOM
    echo "✅ Created src/$f.c"
done

# Generate header files
for f in $modules; do
    guard=$(echo "${f}_H" | tr '[:lower:]' '[:upper:]')
    cat > include/$f.h << EOM
#ifndef $guard
#define $guard

/* TODO: Declare $f functions */

#endif /* $guard */
EOM
    echo "✅ Created include/$f.h"
done

# Generate docs
touch docs/{README,INSTALL,USAGE}.md

# Generate test files
for f in $modules; do
    cat > tests/test_$f.c << EOM
#include <stdio.h>
#include <stdlib.h>
#include "../include/$f.h"

int main() {
    printf("Testing $f...\\n");
    return 0;
}
EOM
    echo "✅ Created tests/test_$f.c"
done

# Make run_tests.sh executable
cat > tests/run_tests.sh << 'EOM'
#!/bin/bash
echo "🧪 Running LinkMate tests..."
for test in tests/test_*.c; do
    gcc -o "${test%.c}" "$test" -Iinclude && ./"${test%.c}"
done
EOM
chmod +x tests/run_tests.sh

# Create basic Makefile
cat > Makefile << 'EOM'
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_GNU_SOURCE
LDFLAGS = -lm
PREFIX = /usr/local

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)
TARGET = linkmate

all: $(TARGET)

$(TARGET): $(OBJS)
$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
$(CC) $(CFLAGS) -Iinclude -c $< -o $@

clean:
rm -f $(OBJS) $(TARGET)

install: $(TARGET)
install -d $(PREFIX)/bin
install -m 755 $(TARGET) $(PREFIX)/bin/

test: $(TARGET)
./tests/run_tests.sh

.PHONY: all clean install test
EOM

echo "✅ Created Makefile"

# Create config file
cat > config/linkmate.conf << 'EOM'
# LinkMate Configuration
[BROWSER]
path = google-chrome
args = --new-tab

[CLIPBOARD]
command = xclip -selection clipboard

[HISTORY]
max_entries = 1000

[UI]
theme = dark
highlight_color = green
EOM
echo "✅ Created config/linkmate.conf"

# Set up environment script
cat > .env << 'EOM'
export LINKMATE_HOME="$HOME/linkmate"
export MODULES="linkmate url_parser terminal browser clipboard history config"
EOM
echo "✅ Created .env"

echo "
🎉 LinkMate project structure created!
Run: source .env
Then: make
Then: ./linkmate --help
"
