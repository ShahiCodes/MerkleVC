# MerkleVC

MerkleVC is a small file-level version control system implemented in C++17. It
uses SHA-1 content identifiers, compressed objects, recursive directory trees,
and a commit graph to record and restore snapshots of a working directory.
The project is intentionally self-contained: the executable is built in the
repository directory and each repository stores its metadata below `.mvc/`.

The implementation has two complementary sides:

- A content-addressed object store for blobs, trees, and commits.
- A command-line layer for snapshots, branches, checkout, restoration, graph
  inspection, status reporting, and three-way merging.

There is no separate index or staging-area implementation. A commit snapshots
the current working directory directly, after excluding MerkleVC's own
metadata and executable.

## Contents

- [Architecture at a glance](#architecture-at-a-glance)
- [Object model and storage](#object-model-and-storage)
- [Repository lifecycle](#repository-lifecycle)
- [Command reference](#command-reference)
- [Plumbing commands](#plumbing-commands)
- [Branching and checkout](#branching-and-checkout)
- [Working-tree safety](#working-tree-safety)
- [Merge engine](#merge-engine)
- [Status, history, graph, and metrics](#status-history-graph-and-metrics)
- [Building and requirements](#building-and-requirements)
- [Tests](#tests)
- [Limitations and implementation notes](#limitations-and-implementation-notes)
- [License](#license)

## Architecture at a glance

The executable is assembled from the modules in `src/` and the public
interfaces in `include/`.

| Layer | Files | Responsibility |
| --- | --- | --- |
| Command dispatch | `src/main.cpp` | Parses commands, validates arguments, and invokes the relevant operation. |
| Repository and objects | `src/repository.cpp` | Initializes `.mvc/`, writes blobs and trees, and compares a working tree with HEAD. |
| Binary and hash utilities | `src/utils.cpp` | Reads and writes bytes, computes SHA-1, compresses/decompresses with zlib, and reports storage metrics. |
| Commit history | `src/commit.cpp`, `src/log.cpp` | Creates commits, maintains HEAD or branch refs, records authors, and walks parent history. |
| Branches | `src/branch.cpp` | Creates, lists, and deletes branch references. |
| Restoration | `src/restore.cpp` | Reads tree/blob objects and reconstructs files in the working directory. |
| Checkout | `src/checkout.cpp` | Resolves a branch or commit target, restores it, and updates HEAD. |
| Merge | `src/merge.cpp` | Finds a common ancestor, performs fast-forward or three-way merging, and records merge state. |
| Status | `src/status.cpp` | Compares the current files with HEAD and reports modified, deleted, and untracked files. |
| Graph | `src/graph.cpp` | Traverses commit parents and prints a terminal graph with branch labels. |

The command dispatcher exposes two broad classes of commands:

- **Porcelain commands** operate on repository history and the working tree:
  `init`, `commit`, `log`, `restore`, `branch`, `checkout`, `merge`, `graph`,
  `status`, and `metrics`.
- **Plumbing commands** expose lower-level object operations:
  `hash-object` and `write-tree`.

## Object model and storage

MerkleVC stores objects by the SHA-1 hash of their serialized representation.
The first two characters of the hash name a directory and the remaining
characters name the object file:

```text
.mvc/objects/<first-two-hash-characters>/<remaining-hash-characters>
```

Every object is compressed with zlib before it is written. The object data is
still prefixed with a textual header containing its type and size, so the hash
identifies both the content and its object type.

### Blob objects

A blob represents the bytes of one regular file. `store_blob` reads the file,
constructs a header in the form `blob <size>\0`, appends the file bytes, hashes
the combined data, compresses it, and writes it to the object store.

A blob does not store its filename or directory. That separation is important:
the same content can be referenced by multiple tree entries without copying the
file bytes into multiple objects.

### Tree objects

A tree represents one directory. Each entry contains:

```text
<mode> <name>\0<20-byte binary object hash>
```

The implementation uses mode `100644` for files and `40000` for directories.
Directories are represented by nested tree objects. `write_tree` walks the
working directory recursively, skips `.mvc`, `.git`, and the `mvc` executable,
sorts entries by name, serializes the entries, hashes the resulting tree, and
stores the compressed object.

Sorting makes the same directory contents produce the same tree representation
regardless of filesystem iteration order. A change to a nested file changes its
blob hash, which changes the containing tree hash and propagates toward the
root tree.

### Commit objects

A commit points to a root tree and contains author and committer metadata,
timestamps, a message, and zero or more parent lines:

```text
tree <root-tree-hash>
parent <parent-commit-hash>
author <name and email> <timestamp>
committer <name and email> <timestamp>

<commit message>
```

Normal commits have one parent when history already exists. The initial commit
has no parent. A completed divergent merge has two parents: the commit that was
checked out when the merge began and the target branch commit recorded in
`.mvc/MERGE_HEAD`.

### Refs and HEAD

Branches are lightweight text files under `.mvc/refs/heads/`. The file contains
the commit hash at the tip of that branch. `.mvc/HEAD` normally contains a
symbolic reference such as:

```text
ref: refs/heads/master
```

When checkout receives a commit hash instead of a branch name, restoration
places the repository in detached HEAD mode by writing the commit hash directly
to `.mvc/HEAD`.

## Repository lifecycle

### Initialization

`mvc init` creates:

```text
.mvc/
├── HEAD
├── config
├── objects/
└── refs/
    └── heads/
```

The command asks for a name and email and stores the resulting identity in
`.mvc/config`. Empty responses fall back to `user <user@example.com>`.

### Committing a snapshot

`mvc commit -m "message"` performs two operations:

1. It recursively writes a tree for the current directory.
2. It creates a commit pointing to that tree and updates the current HEAD or
   branch ref.

The command does not require an `add` step because the current filesystem is
the snapshot source. Any files not excluded by the repository walker become
part of the next commit, including files that `status` calls untracked.

During an in-progress divergent merge, the same commit command consumes
`.mvc/MERGE_HEAD` as the second parent and removes the merge-state files after
the commit is written.

## Command reference

Build the executable first:

```sh
make
```

The binary is then invoked as `./mvc <command> [arguments]`. `mvc help`,
`mvc -h`, and `mvc --help` print the built-in command summary.

### `init`

Initialize a repository in the current directory and configure the author:

```sh
./mvc init
```

Initialization fails if `.mvc/` already exists.

### `commit`

Create a snapshot with a message:

```sh
./mvc commit -m "Initial snapshot"
```

The message must be supplied with `-m`. The command prints the resulting commit
hash and message.

### `status`

Compare the files on disk with the current HEAD snapshot:

```sh
./mvc status
```

Status reports the current branch or detached HEAD, modified tracked files,
deleted tracked files, and untracked files. If a merge is in progress, it also
reports the paths recorded in `.mvc/MERGE_CONFLICTS`.

### `log`

Walk backward through the first parent of the current commit and print commit
metadata and messages:

```sh
./mvc log
```

The log follows the parent chain used by the current HEAD. For full topology,
use `graph`.

### `branch`

List branches, create a branch at the current commit, or delete a branch:

```sh
./mvc branch
./mvc branch feature
./mvc branch -d feature
```

Creating a branch writes a new ref pointing to the current HEAD commit. It does
not copy objects or working files. The current branch is marked in the listing.
The current branch cannot be deleted.

### `checkout`

Switch to a branch or restore a commit in detached HEAD mode:

```sh
./mvc checkout feature
./mvc checkout <commit-hash>
```

For a branch, checkout restores the branch tip and changes `.mvc/HEAD` to the
branch ref. For a commit hash, it restores that commit and leaves HEAD detached.
Checkout refuses to run when tracked working files differ from HEAD or while an
unresolved merge is in progress.

### `restore`

Restore a commit snapshot without selecting a branch:

```sh
./mvc restore <commit-hash>
```

Restoration reads the target commit's tree recursively, writes each blob to
its path, and removes files that were present in the current HEAD but are not
present in the target tree. The command also updates `.mvc/HEAD` to the target
commit. The command is guarded against tracked local changes.

### `merge`

Merge a branch into the currently checked-out branch or detached commit:

```sh
./mvc merge feature
```

The merge command accepts a branch name, not an arbitrary commit argument. It
refuses to begin when tracked files have been modified or deleted. Untracked
files are reported by `status` but are intentionally ignored by the merge
cleanliness check.

### `graph`

Display the reachable commit topology:

```sh
./mvc graph
```

The graph discovers branch tips, follows all recorded parent edges, labels
commits with branch names and HEAD where appropriate, and prints merge commits
with their multiple incoming history paths.

### `metrics`

Inspect the compressed object store:

```sh
./mvc metrics
```

Metrics count readable objects, sum their decompressed size, sum their compressed
on-disk size, and print a calculated compression percentage.

## Plumbing commands

### `hash-object`

Hash and store one file as a blob:

```sh
./mvc hash-object path/to/file.txt
```

The command prints the resulting SHA-1 object ID. It does not create a commit
or add the file to a tree by itself.

### `write-tree`

Recursively write a tree for the current directory or a supplied path:

```sh
./mvc write-tree
./mvc write-tree path/to/directory
```

The command prints the resulting tree object ID without creating a commit.

## Branching and checkout

A typical branch workflow looks like this:

```text
master:  A
              \
feature:       B
```

`mvc branch feature` creates the feature ref at `A`. After checking out the
feature branch and committing, the feature ref advances to `B`, while master
still points to `A`. Switching back restores the files from master and changes
HEAD to point to master again.

Branch creation is therefore cheap because it writes only a reference file.
The objects shared by the branches remain in the same object store.

## Working-tree safety

The repository has two related working-tree comparisons:

- `is_work_tree_clean()` rebuilds a complete tree and is used by restore and
  checkout. It compares the complete current tree representation with the
  current HEAD tree.
- `has_uncommitted_changes()` compares tracked paths and blob hashes. It returns
  true for modified or deleted tracked files and deliberately ignores untracked
  files for merge safety.

Before a merge, the command-line layer and `merge_branch` both enforce the
tracked-file check. This duplicate boundary is intentional: the command cannot
start a merge with local tracked changes, and direct callers of the merge
function receive the same protection.

This guard matters because a merge writes files into the working directory. A
modified tracked file could otherwise be overwritten by an automatic update,
a conflict marker, or a delete/modify result.

## Merge engine

The merge implementation supports three outcomes.

### Already up to date

If the current HEAD and target branch point to the same commit, merge prints
`Already up to date.` and leaves the repository unchanged.

### Fast-forward merge

If the current HEAD is the lowest common ancestor of the target, the current
branch can move directly to the target commit. MerkleVC restores the target
tree, then updates the current branch ref and keeps HEAD attached to that ref.
No merge commit or `MERGE_HEAD` is required.

The branch ref is updated only after restoration succeeds. If restoration fails,
the fast-forward operation reports failure and does not advance the ref.

### Divergent three-way merge

When both sides contain commits after the common ancestor, the merge reads
three file maps:

- **Base:** the tree belonging to the lowest common ancestor.
- **Head:** the tree belonging to the current commit.
- **Target:** the tree belonging to the incoming branch tip.

The lowest common ancestor is found by walking parent edges from both commits
with bidirectional breadth-first traversal. Parent parsing supports commits
with more than one parent, so merge history remains traversable after a merge
commit is created.

For each path present in any of the three snapshots, the merge compares the
base, current, and target blob hashes:

- If current and target are identical, no work is needed.
- If current is unchanged from base, the target version is applied.
- If target is unchanged from base, the current version is retained.
- If one side deleted the path while the other changed it, a delete/modify
  conflict is written.
- If both sides changed the file, the file contents are merged line by line.

### Line-level conflict handling

For changed files, MerkleVC computes LCS-based diff hunks between base and each
side. Non-overlapping edits are combined. When both sides change the same base
region differently, the working file receives a marker block beginning with
`<<<<<<< HEAD`, separating the two versions with `=======`, and ending with
`>>>>>>> target`.

The merge writes the target commit to `.mvc/MERGE_HEAD`. When conflicts exist,
the conflicting paths are written to `.mvc/MERGE_CONFLICTS` and the user must
edit those files before committing.

A clean divergent merge still uses the same two-step lifecycle: the files are
merged first, and the user runs `mvc commit -m "..."` to create the merge
commit. The commit records both parents and removes `.mvc/MERGE_HEAD` and
`.mvc/MERGE_CONFLICTS`.

While merge state exists, checkout refuses to switch branches. This prevents a
partially merged working directory from being silently replaced.

## Status, history, graph, and metrics

These commands provide different views of the same object model:

- `status` compares paths and blob hashes between disk and HEAD. It is the
  working-tree view.
- `log` follows a parent chain and is useful for reading commit messages in
  sequence.
- `graph` follows all parents and is useful for seeing branch divergence and
  merge topology.
- `metrics` scans compressed object files and compares raw object size with
  compressed storage size.

Together these commands make it possible to inspect both the user-facing state
and the underlying commit graph.

## Building and requirements

The project uses GNU Make and compiles all C++ sources as one executable:

```sh
make
```

The `Makefile` uses:

- C++17 and the standard filesystem library.
- OpenSSL's SHA-1 implementation through `-lssl` and `-lcrypto`.
- zlib compression through `-lz`.

A Linux or POSIX-like environment with a C++17 compiler, GNU Make, OpenSSL
development libraries, and zlib development libraries is expected. To remove
the compiled binary:

```sh
make clean
```


## Tests

The repository includes shell-based regression tests for the merge behavior.
Each test creates its own temporary repository under `/tmp`, builds the binary,
and removes its temporary directory after completion.

Run them individually:

```sh
bash tests/merge_test.sh
bash tests/merge_advanced_test.sh
bash tests/merge_uncommitted_test.sh
```

The tests cover:

- Content conflicts and conflict markers.
- `MERGE_HEAD` and `MERGE_CONFLICTS` during conflict resolution.
- Successful merge commits with two parents.
- Fast-forward merges.
- Non-overlapping line-level automatic merges.
- Dirty tracked-file protection before merge.
- Continuing with a merge after the local changes are committed.

The `Makefile` also provides the basic build check:

```sh
make clean
make
```

## Limitations and implementation notes

MerkleVC is a focused educational and experimental tool rather than a complete
replacement for a production version control system. In particular:

- There is no staging index. Every commit snapshots the current working tree.
- Merge accepts branch names and does not expose an abort command. A merge in
  progress must be completed with a commit after conflicts are resolved.
- `log` follows the first parent chain, while `graph` is the topology-oriented
  view for multiple-parent history.
- File modes are represented by the fixed modes used by the tree writer; the
  implementation does not preserve a complete set of filesystem permissions.
- The merge algorithm is text-oriented and uses an in-memory LCS table, so very
  large files may require substantial memory.
- SHA-1 is used as the object identifier because it is part of this project's
  object format; this should not be treated as a modern collision-resistance
  recommendation for new security-sensitive systems.

These constraints are visible in the source and keep the implementation small
enough to study from the command layer down to the serialized objects.



## License

MIT License
