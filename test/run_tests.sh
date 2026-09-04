#!/bin/bash
echo "🧪 Running LinkMate tests..."
for test in tests/test_*.c; do
    gcc -o "${test%.c}" "$test" -Iinclude && ./"${test%.c}"
done
