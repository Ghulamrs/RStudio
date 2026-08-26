# Calling C from Shalimar

A Shalimar program can call a function written in C, compiled by `cc1` (or
`cl`, or the host's compiler) and handed to the link as a library. Arrays cross
the boundary too.

There are worked examples in the two repositories:

| | |
| --- | --- |
| the library | `Compiler-C/examples/shalimar-library` |
| the program | `Compiler-S/examples/using-a-library` |

Build the first, then the second.

## The two forms of `uses`

Shalimar has no library of its own — even `sin` is borrowed — and a file must
say what it borrows before it may call it.

```
uses sin, cos                              // from the table shc carries
uses <real> = stats_mean(a[]: real)        // from a library the link is given
```

The first names functions from the **C standard library**, whose signatures
`shc` already knows. The second **declares** one it has never seen: the
declaration is the whole contract, and `shc` will never learn anything more
about that function.

Both forms are per file, in global space, and a borrowed name is an ordinary
identifier in any file that does not borrow it.

## Writing the C

Ordinary C89. One header, and two rules:

```c
#include "shmrt.h"                 /* Compiler-S/runtime */

double stats_mean(const ShmArray *a)
{
    int32_t n = shm_array_dim(a, 0);
    double  s = 0.0;
    int32_t i;
    for (i = 0; i < n; i++) s += shm_get_real(a, i);
    return n > 0 ? s / n : 0.0;
}
```

**No `main`.** The Shalimar runtime owns it. A second one collides at the link,
and the message is about a duplicate symbol rather than about your file.

**A rank-2 array is nested.** `real[][]` is an array of row *references*.
`shm_get_real` on the outer array reads a reference as a double and returns
nonsense **with no diagnostic at all** — take the row first:

```c
ShmArray *row = shm_get_ref(a, r);
double    v   = shm_get_real(row, c);
```

## Building it

**A compiler does not make a library.** `cc1`, `cl` and `gcc` make *objects*;
`ar` — or `lib.exe` on Windows — makes a library from objects.

```
cc1 -c -I <Compiler-S/runtime> stats.c -o stats.o
ar rcs libstats.a stats.o
```

```
shc prog.shm --with=libstats.a -o prog
```

`--with=` is repeatable and the libraries are given to the linker in the order
written, which is the order a linker cares about.

## What the editor does

Building a Shalimar file from the editor does **not** pass `--with=`. A program
that declares a foreign function therefore has to be built from a terminal, or
from a project whose target names the library. The editor's Shalimar builds are
single files and single commands, and this is the one thing they cannot express.

`shc` says so plainly rather than leaving it to the linker:

```
shc: 'stats_total' is declared with 'uses' and comes from a library, but no
     library was named. Add --with=<path> - see docs/FOREIGN.md.
```

## The limit, and why it is where it is

Only signatures Shalimar's own types can express: `int`, `real`, `char`, and
arrays of them. **Shalimar has no pointers** — the word is not in the
specification — so a C function taking `char *`, returning a pointer, or taking
a variable number of arguments cannot be declared however it is written.

That is the same boundary the `uses sin` form has, and it is not a list
somebody maintains: it falls out of the type system, so `memset`, `strlen`,
`malloc`, `qsort` and `printf` are all outside it for one reason.

## It will not run in the phone app

The app interprets a source file and has no link step, so there is nowhere for
a library to go. It refuses the program by name:

```
Error: line 13: 'stats_total' comes from a library - shc can build this
                program, the app cannot link one
```

This is the only construct in the language that behaves differently in the two
places, and it is recorded in `Compiler-S/docs/CONFORMANCE.md`.

## Where the reasoning is written down

| | |
| --- | --- |
| why `uses` exists at all, and what it refuses | `Compiler-S/docs/FOREIGN.md` |
| how an array crosses, and the accessors | `Compiler-S/docs/ARRAY-ABI.md` |
| why a Shalimar object cannot be a library | `Compiler-S/docs/LINKING.md` |
