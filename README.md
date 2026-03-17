# ToyForth

ToyForth is an interpreted implementation of the Forth programming language.
It does not aim for compliance nor completeness.

This project exists because [@antirez](https://github.com/antirez) asked his
followers to implement it during his C language lessons, though I suppose it
diverged quite a lot, since I purposely skipped a good chunk of lessons to keep
the challenge more interesting.

## Currently supported features

This is an inaccurate but growing list of the features supported by this
implementation.

* Numeric `int` literals
* `+-*/` operators
* Comparison operators
* `if`, `else` and `then`
* `.` dot printing operator
* `cr` prints a new line
* `dup` and `drop`
* `do`, `i`, `leave`, `loop`

## Build & run

The build is system uses a single GNU makefile. Build the program with:

```bash
CFLAGS=-DSTATE_MACHINE_AS_JUMP_LABELS make
```

Simply running `./tf` will enter an interactive shell. Every line is considered
a complete program and runs immediately. To run a saved program just pass it as
command line parameter. Eg.:

```bash
./tf progs/fibonacci.tf
```

## Coding details: how `struct`s are treated

I loosely got influenced by Rust and C++ for memory handling, and I came up with
the following rules:

* Names are pascal case. eg `typedef struct <StructName>_s { ... } <StructName>;`
* Method names carry the struct, plus their own snake case name. eg
  `<StructName>_<method_name>(<StructName> *self, ...)`.
* Initialization is performed preferentially by 
  `<StructName>_init(<StructName> *self, ...)` method, which becomes mandatory
  if complex logic, memory allocation or resource acquisition is needed.
* Finalization is performed by an optional `<StructName>_drop(<StructName> *self)`
  method, which becomes mandatory if resource release is needed.
* It's possible to have a companion typeinfo, lowercase, eg
  `TypeInfo <structname>_typeinfo` as a global variable in the static memory.
  This allows the struct to be used as specialization parameter of generic data
  structures. Eg. `Array` and `Dictionary`.

Since structs are always passed around as pointer, it's not immediately clear
if the struct is being borrowed or transfered for ownership.

Unless where stated, eg in a method doc comment, the rule is:

* `init` and other regular methods are considered to borrow.
* `drop` is considered to retain ownership, consume the structure and make it
  further unusable.

If a method, or any other function, wants to own a struct, it must:

* State it in a doc comment.
* `memcpy` the struct, or the fields relevant to the operation, to
  its destination owner.
* Optionally zero the struct.
* If the struct needs to be consumed, `drop` it.

> A structure that usually needs to release more than one field can support
> dismemberment by making `drop` methods ignore nullified fields, so a method
> interested in changing ownership of a single field can `NULL` it out, and
> call `drop`.

## AI usage

A local 16GB Radeon was stressed with llama.cpp+claude+qwen to review for
memory leaks. No other LLMs were harmed in the making of this project,
but since antirez is incorporating more and more LLM usage in his lessons,
expect the same here.

Anyway, **all code is hand crafted**, except when stated.

