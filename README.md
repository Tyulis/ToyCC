# ToyCC

This is an exploratory project to better understand how compilers and compile-time optimization works.
It's based on C23 and targets x86\_64 assembly

## Scope

This implements a compiler and inserts into the GNU toolchain :

- Preprocessing is done by `cpp`
- The lexer and parser are generated with Antlr4 and a slightly modified version of the [example grammar](https://github.com/antlr/grammars-v4/tree/master/c)
- The compiler targets GNU `as` for x86\_64 assembly

Only the compiler part (semantic analysis, conversion to IR, optimization, assembly code generation) is implemented here.
