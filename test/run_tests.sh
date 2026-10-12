#!/bin/bash
# test/run_tests.sh - Run all LinkMate tests

set -e

echo "🧪 LinkMate Test Suite"
echo "═══════════════════════════════════════"
echo ""

# Build test harness
make -s test-harness

# Run all tests
./test/unit/runner all

echo ""
echo "✅ All tests passed"
