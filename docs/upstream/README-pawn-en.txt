# Advanced Pawn Compiler (APC)

A fork of the Pawn 3.10.8 compiler, optimized for building large SA-MP and open.mp projects.

This compiler was created to speed up the development process. In 2026, there might still be monolithic modifications that have grown beyond 50,000 lines of code.
There might still be Pawn developers in 2026.
APC solves the problem of slow compilation by optimizing the bottlenecks of the original compiler without changing the output file format.

About 75% of the code in this fork was written with the help of AI (Claude and Codex). The author's role in the project was architectural planning, profiling, testing, and ensuring byte-for-byte compatibility with the original. The base source code was provided by [Zeex](https://github.com/Zeex/pawn) and the [pawn-lang](https://github.com/pawn-lang) team — without their work, this project would not have been possible.

## 1. Performance Comparison

Measurements were taken on a synthetic test (~800,000 lines, arrays of 7 million cells, thousands of `switch` branches).

| Compiler Version | Compilation Time | Increase |
| :--- | :--- | :--- |
| Original PawnCC 3.10.8 | 7.20 sec | — |
| ADV Pawn Compiler | 3.10 sec | ~58% (2.38x faster) |

(Screenshot with test results: `bench.png`)

## 2. Main Changes

The `.amx` format and command-line options were not changed. Optimization affected internal algorithms and memory handling:

*   Peephole optimizer (`sc7.c`): In the original, about 150 patterns were iterated over on each line (totaling up to 116 million calls). This took up 67% of the total compilation time. In APC, patterns are indexed by opcode, which reduced the number of calls to ~3 million.
*   Stack analysis: Exponential path traversal in the call graph has been eliminated. Memoization is applied for non-recursive functions. This is crucial for functions with huge `switch` constructs.
*   Removal of unused functions (`reduce_referrers`): Quadratic iteration was replaced with a queue while preserving the logic.
*   Lexer (`sc2.c`): Introduced a candidate table based on the first character of a token (instead of iterating through all 90). Number parsing is skipped for tokens that do not start with a digit. `alphanum` has been converted to a character class table.
*   Assembler (`sc6.c`): Opcode lookup is done through a hash table (without `stricmp`). Opcodes are cached during the first pass and reused in subsequent ones.
*   `.amx` writing: About 40% of instructions in the original were spent on `fwrite` (4 bytes at a time). Now a 256 KB buffer is used, and repeating cells (e.g., zero-initialized arrays) are written in blocks.
*   I/O: Increased the read buffer for source and temporary files (64 KB). Disabled redundant CRT locks (`_CRT_DISABLE_PERFCRIT_LOCKS`).

## 3. Compatibility and Testing

Output files (`.amx`, `.asm`, `.lst`) are byte-for-byte identical to the original compiler 3.10.8. Differences may only be in the version string.

*   Conducted 666 compilations (including tests, examples, `-a`, `-l` modes, various sets of flags) — 0 discrepancies.
*   Built-in protection against incorrect builds: a 64-bit compiler simply will not build.

## 4. Files in the release

*   `apc3.11/` — Advanced Pawn Compiler source code.
*   `build.bat` — Script for building the compiler from source.
*   `sample_compile/` — Synthetic test mod for benchmarking.
*   `bench.bat` — Script for simultaneous verification and comparison of the speed of the old and new compilers.

## 5. Installation

Simply replace `pawncc.exe` and `pawnc.dll` in your Pawno folder. The SA-MP or open.mp server will not notice the difference, as the bytecode remains the same.