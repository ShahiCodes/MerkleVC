# MerkleVC

[> Skip to TL;DR (Tech Stack & Architecture)](https://www.google.com/search?q=%23tldr)

MerkleVC is a file-level version control system built from the ground up in C++. It is based on the fundamental principle of a Merkle Directed Acyclic Graph, where the integrity of the entire repository is guaranteed because every subgraph is a recursive cryptographic hash of its children. By treating the filesystem as a Content-Addressable Storage system rather than a simple hierarchy of files, MerkleVC ensures O(1) deduplication of identical content, efficient zlib-based compression, and extremely low-latency execution for common operations.

This project implements a custom 3-Way Merge algorithm capable of detecting the Lowest Common Ancestor (LCA) in a complex history graph, allowing it to intelligently resolve divergent branches and integrate them without data loss.

## The Core Architecture

The working repository created during it's use is a persistent object store. The system is driven by three distinct types of immutable objects that link together to form the history of your project.

**Blob Objects** are the raw content of your files. When you add a file to the system, it is compressed and hashed. If two files in different directories have the exact same content, they point to the same Blob hash, ensuring zero storage redundancy.

**Tree Objects** represent the directory structure. A Tree maps filenames to their corresponding Blob hashes or other Tree hashes. Because Trees are hashed based on their content, a change in a deeply nested file ripples up the chain, changing the hash of the Root Tree and ensuring the entire snapshot is unique.

**Commit Objects** provide the context. They wrap a Root Tree hash with metadata (author, timestamp, log message) and crucially, the hash of the parent commit. This linking of parent hashes is what forms the DAG, allowing us to trace history back to the very first transaction.

## Selective Staging and Recursion

MerkleVC employs a precise staging environment to control exactly what goes into a snapshot. Rather than capturing the entire working directory indiscriminately, the system allows users to explicitly target files for the next transaction.

This mechanism is powered by a robust **recursive tracker**. When a directory is targeted, the system automatically traverses the folder structure, indexing all valid nested files and subdirectories. This allows for flexible workflows where users can stage individual files, specific sub-folders, or the entire project in a single command. The system maintains a persistent index of these tracked files, ensuring that subsequent status checks and commits only monitor the content you have explicitly chosen to manage.

## Low-Level Plumbing

While most users interact with high-level commands, MerkleVC exposes the low-level "plumbing" commands that power the engine. These are critical for debugging and understanding the internal state of the DAG.

The `hash-object` command allows you to verify how the system sees a specific file by manually computing its SHA-1 hash and writing it to the object store. Conversely, `write-tree` lets you manually snapshot the current staging area into a Tree object without creating a commit. Implementing these primitives was necessary to decouple the data storage layer from the history management layer, allowing each component to be tested and optimized in isolation.

## Branching and Restoration

Branching in this system is lightweight and virtually instantaneous. Creating a branch does not duplicate files or history; it simply creates a moveable pointer (a "Ref") that points to a specific commit hash. When you switch branches, you are merely moving this pointer and asking the system to align the working directory with the snapshot that the pointer references.

However, moving between snapshots is dangerous if you have unsaved changes (during the developement I lost some chunk of my code because I was testing commits and there was no safety check constraints, thus I realised it's need for implementation). The system implements strict "Safety Checks" before any `restore` or `checkout` operation. It compares the hash of your current working files against the HEAD commit. If modifications are detected that would be overwritten by the restore, the operation aborts immediately. This ensures that the user never accidentally loses work during a context switch.

## The Merge Engine

The core of this project is its ability to handle non-linear history through a robust merge engine.

It’s a two-pass ancestor-walk algorithm using a set to detect the first common ancestor.

In simple scenarios, the system performs a **Fast-Forward Merge**. If the target branch is directly ahead of the current branch—conceptually functioning like a linked list—the system simply slides the current branch pointer forward to the new commit. No new merge commit is required because history has not diverged.

The complexity arises when history splits. If two branches have evolved independently, the system triggers a **3-Way Merge**. This algorithm first performs a two-pass ancestor-walk algorithm using a set to detect the Lowest Common Ancestor (LCA), the last point where the two branches shared a history. It then compares three snapshots simultaneously: the Base (LCA), the Head (Current), and the Target (Incoming). By analyzing how both children have deviated from the parent, the system can automatically combine distinct changes or halt execution if a conflicting modification is detected in the same file.

It is important to note that MerkleVC operates as a File-Level system, distinct from line-level tools. The merge engine treats files as atomic blobs and it detects changes by comparing the SHA-1 hash of the entire file. It does not parse the internal text content of files (diffing lines).

In case of a merge-conflict the user has to manually resolve it.

## Visualizing the Graph

History is rarely a straight line. To make sense of branches, merges, and divergence, MerkleVC includes a native `graph` visualization command. This renders the commit history directly in the terminal as a recursive tree. It allows you to see exactly where branches split, where they rejoin, and the dual-parent lineage of merge commits, providing an immediate visual verification of the repository's topology.

## Command Reference

The tool suite is divided into operations for managing state, history, and analysis.

**Repository Management**

* `init`: Initializes a new empty repository and object database.
* `add`: Stages specific files or entire directories for the next commit, enabling recursive tracking of new content.
* `status`: Scans the working tree and reports modified, deleted, or untracked files.
* `metrics`: Displays performance analytics, including object count and compression efficiency.

**History & State**

* `commit`: Captures the staged tree and creates a new commit object.
* `restore`: Updates a specific file in the working directory to match the version in HEAD.
* `log`: Prints the linear commit history of the current branch.

**Branching & Merging**

* `branch`: Lists existing branches or creates a new one at the current HEAD.
* `checkout`: Safely switches the working directory and HEAD pointer to a different branch.
* `merge`: Integrates a target branch into the current branch using 3-way logic.
* `graph`: Visualizes the DAG structure of the repository.

**Plumbing**

* `hash-object`: Computes the SHA-1 hash of a file and writes it to the database.
* `write-tree`: Recursively captures the directory structure as a Tree object.

## Installation and Usage

You can run MerkleVC on Linux system without needing root access or global installation. The recommended approach is to build the binary locally within the project folder.

1. Clone the repository to your local machine.
2. Run `make` in the project root to compile the `mvc` binary.
3. You can now use the system immediately by running `./mvc <command>` (e.g., `./mvc init`) directly inside that folder.

There is no need to modify your system's `PATH` or install headers globally. The binary is self-contained and operates strictly within the directory where it is initialized.

---

## <a name="tldr"></a>TL;DR: Project Summary

### Tech Stack

* **Language:** C++17 (Standard Template Library, Filesystem API)
* **Crypto:** OpenSSL (SHA-1 Hashing)
* **Compression:** Zlib (DEFLATE algorithm)
* **Build System:** GNU Make
* **Environment:** Linux / POSIX

### Architecture

* **Merkle DAG:** Uses a directed acyclic graph where every node is uniquely identified by its cryptographic hash.
* **Content-Addressable Storage:** Files are stored based on their content, not their location, enabling O(1) deduplication.
* **Object Model:** History is composed of linked `Commit` -> `Tree` -> `Blob` objects.

### Key Features

* **Sub-millisecond Latency:** Optimized for high-throughput transactional speed.
* **Recursive Staging:** Smartly indexes nested directories and tracks selected files for committing.
* **3-Way Merge:** Implements a file level version control system for automatic conflict resolution in divergent branches.
* **Safety Checks:** Prevent data loss during checkouts.
* **Visualizer:** Built-in terminal graph renderer to visualize commit topology.
* **Compression:** Achieves significant reduction in disk usage via transparent zlib compression.

### License

MIT License