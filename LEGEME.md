# CCC — Compilator C Completus

Compilator C99 in C99 puro, sine ullis dependentiis externis. Generat
codicem ARM64, scribit Mach-O directe. Nulla assemblatrix externa,
nullus ligator externus.

> **Nota:** README.md ignorandum est. Anglice scriptum est ad barbaros ineruditos pecunia emungendos.

## Aedificatio

```
face
```

Hoc `ccc` aedificat.

## Usus

```
ccc plica.c -o exitus
```

Plicam C99 compilat et executabile Mach-O ARM64 generat. Post
generationem, `codesign -s -` automatice vocatur.

## Architectura

| Plica | Descriptio |
|---|---|
| `ccc.h` | caput commune — typi, constantiae, declarationes |
| `ccc.c` | principale — `main()`, lectio plicarum, errores |
| `lexator.c` | lexator et praeprocessor — signa, macrae, #include |
| `parser.c` | parser descendens recursivus — AST |
| `genera.c` | generans codicis ARM64 + scriptor Mach-O |

## Pipeline

1. Lexator dissect fontem in signa (tokens)
2. Praeprocessor tractat `#include`, `#define`, `#ifdef`
3. Parser producit arborem syntaxis abstractam (AST)
4. Generans ambulat AST et emittit instructiones ARM64
5. Scriptor componit Mach-O: header, load commands, sectiones, LINKEDIT
6. `codesign -s -` signat executabile

## Proprietates

- Typi: `void`, `char`, `short`, `int`, `long`, `unsigned`, indicis, tabulae, structurae, enumerationes
- Sententiae: `if`/`else`, `while`, `do`/`while`, `for`, `switch`/`case`, `return`, `break`, `continue`
- Omnes operatores C: arithmetici, logici, relationales, bitwise, assignatio
- Praeprocessor: `#include`, `#define`, `#ifdef`/`#ifndef`/`#endif`
- Capita interna pro functionibus systematis (stdio, stdlib, string, termios, ioctl)
- Ligatio dynamica cum libSystem.B.dylib per GOT

## Limites

- Solum ARM64 macOS (Apple Silicon)
- Nullum floating-point
- Nullae uniones
- Nullae function pointers ut valores
- Optimizatio minima

## Licentia

Liberum. Dominium publicum. Utere quomodo vis.
