#!/bin/bash
set -e

# Build and get binary path
cd "$(dirname "$0")/.."
make clean
make
BIN="$(pwd)/mvc"

# Create test directory
TEST_DIR="/tmp/mvc_merge_uncommitted_$$"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"
OUTPUT_LOG="/tmp/mvc_merge_uncommitted_$$.log"

echo "=== Initializing repository ==="
echo "Test User" | "$BIN" init
"$BIN" commit -m "Initial commit"

echo "=== Create a file and commit it ==="
echo "Base content" > file.txt
"$BIN" commit -m "Add file.txt"

echo "=== Create and switch to a feature branch ==="
"$BIN" branch feature
"$BIN" checkout feature
echo "Feature content" > file.txt
"$BIN" commit -m "Feature commit"

echo "=== Switch back to master ==="
"$BIN" checkout master

echo "=== Modify file without committing (uncommitted changes) ==="
echo "Uncommitted modification" > file.txt

echo "=== Attempt merge (must be blocked by the safety check) ==="
set +e
"$BIN" merge feature > "$OUTPUT_LOG" 2>&1
MERGE_EXIT=$?
set -e

if [ $MERGE_EXIT -eq 0 ]; then
    echo "ERROR: Merge succeeded despite uncommitted changes"
    exit 1
fi

if ! grep -q "Cannot merge with uncommitted changes" "$OUTPUT_LOG"; then
    echo "ERROR: Expected error message not found"
    exit 1
fi

echo "Uncommitted changes check OK"

echo "=== Commit the changes ==="
"$BIN" commit -m "Commit uncommitted changes"

echo "=== Now merge must pass the safety check ==="
"$BIN" merge feature > "$OUTPUT_LOG" 2>&1
if grep -q "Cannot merge with uncommitted changes" "$OUTPUT_LOG"; then
    echo "ERROR: Merge still blocked by uncommitted changes check after committing"
    exit 1
fi

echo "Merge proceeded after committing changes"
echo "All uncommitted changes safety test passed."

# Cleanup
cd /
rm -rf "$TEST_DIR"
