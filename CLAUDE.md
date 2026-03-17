# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ToyForth is an interpreted implementation of the Forth programming language. It's designed as a learning exercise inspired by a challenge from @antirez during his C language lessons. The project focuses on implementing core Forth concepts without aiming for full compliance or completeness.

## Key Components

### Core Files
- `tf.c` - Main interpreter implementation with parser and interpreter logic
- `abstractions.c` / `abstractions.h` - Core data structures (Array, Dictionary, String) used throughout the project
- `test.c` - Basic test suite for core abstractions
- `makefile` all lowercase - The GNU make build instructions

### Architecture
The codebase implements a stack-based interpreter with:
1. **Parser** (`TfParser`) - Tokenizes input into elements (numbers, operators, words)
2. **Interpreter** (`TfInterpreter`) - Executes parsed elements using a result stack and scope management
3. **Memory Management** - Uses custom abstractions for dynamic arrays and dictionaries

### Memory Management Pattern
The project follows a specific memory management approach:
- Structs are always passed as pointers
- `init` methods borrow (don't take ownership)
- `drop` methods consume/retain ownership
- Structs can be copied via `memcpy` when needed
- Type information (`TypeInfo`) is used for generic data structures

## Build and Test Commands

To build the project:
```bash
make
```

To build with support for jump labels (preferred):
```bash
CFLAGS=-DSTATE_MACHINE_AS_JUMP_LABELS make
```

To run tests:
```bash
make test
```

To clean build artifacts:
```bash
make clean
```

## Development Workflow

1. Use `make` to compile the interpreter (`tf`)
2. Run the interpreter with `./tf` for interactive Forth session
3. Tests can be run with `./test`
4. The codebase uses a custom memory management system based on arrays and dictionaries

## Key Data Structures

- **Array**: Generic dynamic array implementation in abstractions.c/h
- **Dictionary**: Hash table implementation using Array internally
- **String**: Null-terminated string type with length tracking
- **TfElement**: Represents parsed tokens (numbers, operators, words, etc.)
- **TfScope**: Stack management for conditional execution (`if`/`else`/`then`)

## Doc Comment Format

Functions have simple doc comments using the C-style `/* ... */` format:

**Single-line comments** for simple descriptions:
```c
/*
 * Computes a string hash.
 */
uint32_t String_hash(char *c_str) {
```

**Multi-line comments** with parameters and return values:
```c
/*
 * Initializes a Dictionary with the given typeinfo.
 * The dictionary uses a hash table with power-of-2 sized buckets.
 * The entry_typeinfo is pinned in place (won't move) to allow safe
 * access from buckets.
 * Returns true on success, false on failure.
 */
bool Dictionary_init(Dictionary *self, TypeInfo *typeinfo) {
```

**Multi-line comments with parameters section:**
```c
/*
 * Drops an Array and frees allocated memory.
 * Parameters:
 * self: the array to drop
 * Returns: none (void)
 */
void Array_drop(Array *self) {
```

Key rules:
- First line is the description
- Use `* Parameters:` followed by parameter descriptions
- Use `* Returns:` for return value description
- Parameters are listed as `* paramname: description`
- Blank line at start of comment
