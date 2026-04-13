# CCC — A Complete C Compiler in C

A self-contained C99 compiler that generates native ARM64 machine code and writes Mach-O executables directly — no assembler, no linker, no external tools. Give it a `.c` file, get a running binary. The entire compiler fits in four source files and compiles in under a second.

Every other small C compiler either targets a virtual machine, emits assembly text for `as` to process, or shells out to `ld` for linking. CCC does none of that. It reads C source, parses it into an AST, generates ARM64 instructions as raw bytes, constructs a complete Mach-O executable with proper load commands, dynamic linking stubs, and symbol tables, then writes it to disk. The output runs natively on Apple Silicon with no intermediary steps.

## What You Get

The full compilation pipeline in a single binary:

- **C99 lexer and preprocessor** — tokenization, `#include`, `#define`, `#ifdef`/`#ifndef`, macro expansion, string concatenation, all escape sequences
- **Recursive descent parser** — declarations, statements, expressions with correct operator precedence, structs, enums, typedefs, function definitions
- **ARM64 code generator** — native machine code emission, register allocation, function call ABI compliance, pointer arithmetic, struct layout with correct alignment
- **Mach-O writer** — complete executable format: headers, segments, sections, GOT for dynamic linking, bind info, export trie, symbol table, string table
- **Built-in system headers** — stdio, stdlib, string, termios, ioctl, errno — everything needed to compile real programs that talk to the OS
- **Automatic code signing** — `codesign -s -` runs automatically so the binary executes immediately on Apple Silicon

## Building

```bash
make -f Faceplica        # builds ccc
make -f Faceplica purga  # clean
```

## Usage

```bash
./ccc program.c -o program
./program
```

That's it. No flags to memorize, no build system to configure, no toolchain to install. One command turns C source into a running program.

## Architecture

The compiler is split into four focused modules: the lexer handles tokenization and preprocessing, the parser builds an AST with full type information, the code generator walks the AST to emit ARM64 instructions into a byte buffer, and the Mach-O writer arranges everything into the executable format the kernel expects. Each module is a single `.c` file. The shared header defines all types. There are no circular dependencies, no global state leaking between modules, no abstraction layers for the sake of abstraction.

## Why CCC

Because a C compiler shouldn't need a C compiler toolchain. CCC produces executables that are byte-for-byte correct Mach-O binaries — the same format that `clang` and `ld` produce, with proper dynamic linking to `libSystem.B.dylib` for all standard library functions. The generated code runs at native speed because it *is* native code, not interpreted bytecode or JIT-compiled IR.

The built-in system headers mean CCC can compile programs that call `malloc`, `printf`, `read`, `write`, `tcsetattr`, `ioctl` — real systems programming functions — without needing access to the platform SDK headers. The compiler knows the ABI, the struct layouts, the calling conventions. It speaks the same language the kernel does.

## License

Free. Public domain. Use however you like.
