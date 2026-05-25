# Smart Code Autocomplete Engine (C++)

The Smart Code Autocomplete Engine is a C++ project designed to suggest intelligent code completions in interactively — similar to how modern IDEs like VS Code or IntelliJ offer autocomplete suggestions.
It applies core Data Structures and Algorithms (DSA) concepts such as Tries, Heaps, and LRU (Least Recently Used) caching to efficiently predict the next most probable code tokens based on user input frequency and context.

## Idea
When a programmer starts typing part of a keyword or function name, the engine quickly:
Searches through a Trie (prefix tree) for all words starting with that prefix.
Uses a Heap to rank suggestions by frequency or relevance.
Employs an LRU Cache to prioritize recently used or selected completions, making the system adaptive over time.

This results in fast, memory-efficient, and intelligent autocomplete suggestions that simulate the logic behind real-world code editors — but built purely from scratch using fundamental DSA concepts.


## DSA Concepts used

### 1. Trie (Prefix Tree)

- Files: tst.h, tst.cpp, tst_test.cpp
- Used for storing and retrieving words efficiently based on their prefixes.
- Enables O(L) time complexity lookups (where L = length of prefix).
- Supports real-time suggestions as the user types each character.

🔹 Concepts used: String manipulation, recursion, tree traversal, prefix-based searching.

### 2. Min-Heap / Max-Heap

- Files: minheap.h, minheap.cpp, heap_test.cpp
- Maintains the top N most frequent or relevant words efficiently.
- Provides O(1) access to the top-ranked suggestion with O(log k) updates.
- Used during ranking and sorting of autocomplete results.

🔹 Concepts used: Binary heap operations, priority queue logic, partial sorting.

### 3. LRU (Least Recently Used) Cache
- Files: lru.h, lru.cpp, lru_test.cpp
- Stores recently used suggestions for quick access.
- Improves responsiveness by avoiding repetitive Trie lookups.
- Implemented using a combination of doubly linked list + hash map.

🔹 Concepts used: Linked lists, hashing, cache eviction policy.

### 4. KMP (Knuth–Morris–Pratt) Algorithm
- Files: kmp.h, kmp.cpp
- Used for efficient substring pattern matching between typed input and stored code tokens.
- Ensures fast lookup of partial matches even in large word lists.

🔹 Concepts used: Prefix table computation, linear-time pattern searching.

### 5. Graph Data Structure

- Files: graph.h, graph.cpp
- Represents relationships between tokens or code components.
- Models token co-occurrence relationships to enable context-aware autocomplete suggestions.

🔹 Concepts used: Adjacency list representation, graph traversal (BFS/DFS).

### 6. Stack

- Files: stack.h, stack.cpp
- Used to implement undo and redo functionality in the editor.
- Stores previously accepted tokens or editing states for controlled rollback.

🔹 Concepts used: LIFO operations, state management, undo/redo logic.


### 7. Ranking System
- Files: ranker.h, ranker.cpp
- Combines frequency and recency scores from Trie, Heap, and LRU cache to rank autocomplete suggestions.
- Implements a weighted scoring system for realistic, adaptive predictions.

🔹 Concepts used: Comparator functions, dynamic sorting, frequency-based ranking.

### 8. Frequency Storage
- Files: freq_store.h, freq_store.cpp, frequency.txt
- Keeps track of how often each word is used.
- Updates dynamically after every suggestion selection, making the model “learn” over time.

🔹 Concepts used: File handling, hash mapping, frequency analysis.

---

## Features
- Insert code keywords or phrases  
- Autocomplete suggestions based on prefix  
- Suggestions ranked by frequency  
- Snippet support (e.g., `fori` → `for (int i = 0; i < n; i++)`)  
 - Combined suggestion pipeline — learned phrases first, then TST prefix matches, then KMP substring matches as a fallback; returns up to 10 suggestions.
 - Top-K ranking uses a MinHeap fed by the Ranker (frequency + co-occurrence boost); results are cached in an LRU keyed on the typed prefix.
 - Undo/Redo (Ctrl+Z / Ctrl+Y) over whole-document snapshots, so edits that change the line count are reversible.
 - Frequency counts persist to disk on a background writer thread, keeping file I/O off the keystroke path.
- Practical demonstration of Trie + Hash Map + Heap working together

## Tech Stack Used

| Category              | Technologies / Tools                                | Description                                                                      |
| --------------------- | --------------------------------------------------- | -------------------------------------------------------------------------------- |
| **Language**          | C++ (C++17 Standard)                                | Core implementation of all modules, data structures, and algorithms.             |
| **Build System**      | GNU Make (Makefile)                                 | Used for compiling multiple source files and linking into a single executable.   |
| **Compiler**          | GCC / G++                                           | To compile and build the C++ source files efficiently.                           |
| **Version Control**   | Git + GitHub                                        | For code management, versioning, and collaboration among team members.           |
| **Testing Framework** | Custom test files (`tests/`)                        | Unit testing for core modules like Trie, LRU, and Heap.                          |
| **Data Storage**      | Text files (`data/words.txt`, `data/frequency.txt`) | Stores training words and their frequency for suggestion ranking.                |
| **Editor / IDE**      | VS Code                                             | Primary development environment for coding, debugging, and project organization. |

---


## How It Works
1. User enters keywords (e.g., `print`, `printf`, `private`, etc.)
2. Trie stores all words for fast prefix lookup
3. Hash map tracks how often each token is selected
4. Heap ranks the top-K most relevant matches for the prefix
5. Snippets expand small abbreviations into full code blocks

## Example Usage
- Type: `pri`
- Suggestions: `print`, `printf`, `private` (ranked)
- Type: `fori`
- Expanded snippet: `for (int i = 0; i < n; i++)`

## Setup Instructions
---
### 1. Clone the repository
- git clone https://github.com/maahi271005/Smart-Code-Autocomplete-Engine-DSA-Project
---


### 2. (Optional) Create and activate virtual environment


- python3 -m venv venvname
- source venvname/bin/activate

---

### 3. Install build tools (for Linux/Ubuntu)
- sudo apt update
- sudo apt install build-essential g++ libncurses-dev

---
### 4. Build the project


Use the root `Makefile`. 
Build the terminal editor:

```bash
make
# produces ./basic_editor
```
If you prefer to compile the editor manually, in the terminal:

```bash
g++ -std=c++17 basic_editor.cpp \
	src/tst.cpp src/phrase_store.cpp src/freq_store.cpp src/ranker.cpp src/graph.cpp \
	src/minheap.cpp src/lru.cpp src/stack.cpp src/kmp.cpp \
	-lncurses -Iinclude -o basic_editor
```

---
### 5. Run the program

- Run the editor:

```bash
./basic_editor
```


Notes:
- `scratch/` is created automatically by `basic_editor` and is ignored by git; editor-saved local files will go there by default.
- If you downloaded pre-built binaries and see errors about GLIBCXX or GLIBC versions, rebuild locally (e.g., `make clean && make`) to link against your machine's C++ runtime.
- A separate CLI-based autocomplete tester exists for development and debugging.
It is not required to run the ncurses-based editor and is excluded from the default build.


---
## Running Tests

```bash
make test
```

Builds and runs all four suites. Each test links only the modules it exercises —
the implementations live in `src/`, so compiling a test file on its own will
fail at link time.

To run one suite by hand, link its implementation alongside it:

```bash
g++ -std=c++17 -Iinclude tests/heap_test.cpp src/minheap.cpp -o heap_test && ./heap_test
g++ -std=c++17 -Iinclude tests/lru_test.cpp  src/lru.cpp     -o lru_test  && ./lru_test
g++ -std=c++17 -Iinclude tests/tst_test.cpp  src/tst.cpp     -o tst_test  && ./tst_test
```

`tests/regression_test.cpp` pins down three defects found while profiling, so
they cannot silently return:

| Test | Guards against |
| ---- | -------------- |
| `prefixSearch` bound | Collecting every match under a prefix before truncating to k |
| `FreqStore` concurrency | Lost updates and the recursive `shared_mutex` acquire in `bump()` |
| `UndoRedoStack` round-trip | A redo that replays the state undo just restored |

---
## Performance

Measured on the suggestion path, averaged over repeated lookups:

| Operation | Cost |
| --------- | ---- |
| Suggestion latency (87-token dictionary) | ~23–56 µs |
| `prefixSearch`, 40 000 matching words | 0.44 µs |
| `prefixSearch`, 10 matching words | 0.46 µs |
| LRU cache hit | 0.06 µs |
| `bump()` (frequency update) | 0.04 µs |

Two results worth calling out:

**Prefix lookup no longer depends on how many words match.** `collectWords`
originally walked the entire subtree under a prefix and the caller discarded all
but `k`, so a common prefix cost O(total matches). Against a synthetic
100 000-token dictionary that was 1 155 µs per lookup — slower than a linear
scan. Bounding the traversal brought it to 1.8 µs, a 656× improvement.

**Frequency persistence is off the keystroke path.** `bump()` used to rewrite the
whole frequency file inline on every accepted suggestion. It now updates the map
under an exclusive lock and signals a background writer, which coalesces any
number of bumps into a single snapshot write. The snapshot is copied under a
shared lock and written after releasing it, so disk I/O never blocks a reader.

---
## Applications
- Code Editors (VS Code, JetBrains)
- Search Engines
- Chatbots
- AI-assisted development tools

---


## Contributors 

| Name               | Roll Number | GitHub                                                                                                                       |
| ------------------ | ----------- | ---------------------------------------------------------------------------------------------------------------------------- |
| Tanisha Ray        | B24CM1061   | [![GitHub](https://img.shields.io/badge/-@coderTanisha22-181717?logo=github&style=flat)](https://github.com/coderTanisha22)      |
| Maahi Ratanpara    | B24CS1040   | [![GitHub](https://img.shields.io/badge/-@maahiratanpara-181717?logo=github&style=flat)](https://github.com/maahi271005)     |
| Anika Sharma       | B24CM1009   | [![GitHub](https://img.shields.io/badge/-@anikasharma-181717?logo=github&style=flat)](https://github.com/Anika438)           |
| Akshita Maheshwari | B24CM1006   | [![GitHub](https://img.shields.io/badge/-@akshitamaheshwari-181717?logo=github&style=flat)](https://github.com/AkshitaM1234) |


---
## Purpose
This project demonstrates the application of DSA in a real-world scenario - showing how core structures like tries, heaps, and caches can combine to form an intelligent system used in everyday developer tools.
#test



