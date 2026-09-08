#!/bin/bash
set -e

# Build and get binary path
cd "$(dirname "$0")/.."
make clean
make
BIN="$(pwd)/mvc"

# Create test directory
TEST_DIR="/tmp/mvc_merge_advanced_$$"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"
OUTPUT_LOG="/tmp/mvc_merge_advanced_$$.log"

echo "=== Initializing repository ==="
echo "Test User" | "$BIN" init
"$BIN" commit -m "Initial commit"

# ---- Test 1: Fast-forward merge ----
echo "=== Test 1: Fast-forward merge ==="
echo "Initial content" > file1.txt
"$BIN" commit -m "Add file1.txt"

"$BIN" branch feature
"$BIN" checkout feature
echo "Feature change" > file1.txt
"$BIN" commit -m "Feature commit"

"$BIN" checkout master
"$BIN" merge feature > "$OUTPUT_LOG" 2>&1
if ! grep -q "Fast-forward" "$OUTPUT_LOG"; then
    echo "ERROR: Fast-forward not detected"
    exit 1
fi

if [ -f .mvc/MERGE_HEAD ]; then
    echo "ERROR: MERGE_HEAD exists after fast-forward"
    exit 1
fi

if ! grep -q "Feature change" file1.txt; then
    echo "ERROR: Fast-forward did not update file"
    exit 1
fi
echo "Fast-forward merge OK"

# ---- Test 2: Auto-merge (non-overlapping) ----
echo "=== Test 2: Auto-merge (non-overlapping) ==="
"$BIN" branch feature2
"$BIN" checkout feature2
echo "Line A
Line B
Line C" > file2.txt
"$BIN" commit -m "Add file2.txt"

"$BIN" branch feature2a
"$BIN" branch feature2b

"$BIN" checkout feature2a
echo "Line A modified in a
Line B
Line C" > file2.txt
"$BIN" commit -m "Modify line A"

"$BIN" checkout feature2b
echo "Line A
Line B
Line C modified in b" > file2.txt
"$BIN" commit -m "Modify line C"

"$BIN" checkout feature2a
"$BIN" merge feature2b > "$OUTPUT_LOG" 2>&1
if grep -q "CONFLICT" "$OUTPUT_LOG"; then
    echo "ERROR: Unexpected conflict in non-overlapping merge"
    exit 1
fi
if [ ! -f .mvc/MERGE_HEAD ]; then
    echo "ERROR: MERGE_HEAD missing before clean merge commit"
    exit 1
fi

EXPECTED="Line A modified in a
Line B
Line C modified in b"
if [ "$(cat file2.txt)" != "$EXPECTED" ]; then
    echo "ERROR: Non-overlapping merge output mismatch"
    exit 1
fi
"$BIN" commit -m "Merge feature2b"
if [ -f .mvc/MERGE_HEAD ]; then
    echo "ERROR: MERGE_HEAD remains after clean merge commit"
    exit 1
fi
echo "Non-overlapping auto-merge OK"

# ---- Test 3: Clean two-parent merge (non-conflicting) ----
echo "=== Test 3: Clean two-parent merge (different changes) ==="
"$BIN" checkout master
echo "Base
Same
Context" > file3.txt
"$BIN" commit -m "Add file3.txt"

"$BIN" branch branchA
"$BIN" branch branchB

"$BIN" checkout branchA
echo "Base
Modified in A
Context" > file3.txt
"$BIN" commit -m "Change line 2 in A"

"$BIN" checkout branchB
echo "Base
Same
Modified in B" > file3.txt
"$BIN" commit -m "Change line 3 in B"

"$BIN" checkout branchA
"$BIN" merge branchB > "$OUTPUT_LOG" 2>&1
if grep -q "CONFLICT" "$OUTPUT_LOG"; then
    echo "ERROR: Conflict in two-parent merge"
    exit 1
fi
if [ ! -f .mvc/MERGE_HEAD ]; then
    echo "ERROR: MERGE_HEAD missing before clean merge commit"
    exit 1
fi

EXPECTED="Base
Modified in A
Modified in B"
if [ "$(cat file3.txt)" != "$EXPECTED" ]; then
    echo "ERROR: Two-parent merge output mismatch"
    exit 1
fi
"$BIN" commit -m "Merge branchB"
if [ -f .mvc/MERGE_HEAD ]; then
    echo "ERROR: MERGE_HEAD remains after clean merge commit"
    exit 1
fi
echo "Clean two-parent merge OK"

# ---- Cleanup ----
cd /
rm -rf "$TEST_DIR"

echo "All advanced merge tests passed."