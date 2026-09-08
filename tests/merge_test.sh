#!/bin/bash
set -e

# Build the binary
cd "$(dirname "$0")/.."
make clean
make
BIN="$(pwd)/mvc"

# Create a clean test directory
TEST_DIR="/tmp/mvc_merge_test_$$"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

echo "=== Initializing repository ==="
echo "Test User" | "$BIN" init
"$BIN" commit -m "Initial commit"

# Create a file with initial content
echo "Line 1
Line 2
Line 3" > file.txt
"$BIN" commit -m "Add file.txt"

# Create a branch and make changes
"$BIN" branch feature
"$BIN" checkout feature
echo "Line 1
Line 2 changed in feature
Line 3" > file.txt
"$BIN" commit -m "Feature change"

# Switch back to master and make different changes
"$BIN" checkout master
echo "Line 1
Line 2 changed in master
Line 3" > file.txt
"$BIN" commit -m "Master change"

echo "=== Testing merge with conflict ==="
"$BIN" merge feature || true  # merge may report conflict, but should not abort

# Check that MERGE_HEAD exists
if [ ! -f .mvc/MERGE_HEAD ]; then
    echo "ERROR: MERGE_HEAD not created on conflict"
    exit 1
fi

# Check conflict markers in file.txt
if ! grep -q "<<<<<<< HEAD" file.txt; then
    echo "ERROR: Conflict markers not found in file.txt"
    exit 1
fi

# Verify status shows unmerged
if ! "$BIN" status | grep -q "Unmerged"; then
    echo "ERROR: status does not show unmerged"
    exit 1
fi

# Resolve conflict by taking master's version
sed -i '/<<<<<<< HEAD/,/>>>>>>> target/d' file.txt
echo "Line 1
Line 2 changed in master
Line 3" > file.txt
"$BIN" commit -m "Merge resolved"

# Check that MERGE_HEAD is removed after commit
if [ -f .mvc/MERGE_HEAD ]; then
    echo "ERROR: MERGE_HEAD still exists after commit"
    exit 1
fi

# Clean up
cd /
rm -rf "$TEST_DIR"

echo "All tests passed."