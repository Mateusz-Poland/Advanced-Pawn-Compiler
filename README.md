# Advanced Pawn Compiler (APC)

**Advanced Pawn Compiler (APC)** is a performance-oriented fork of the Pawn compiler, intended for large Pawn codebases, including SA-MP and open.mp projects.

> **Development status:** this repository snapshot is a development build for the 3.11.0 line, not a final stable release. Review the known limitations and run your own compatibility tests before using it in production.

## What is included

- `pawncc` — command-line Pawn compiler.
- `pawnc` — compiler shared library (`pawnc.dll` on Windows, `libpawnc.so` on Linux).
- `pawndisasm` — AMX disassembler.
- `include/`, `examples/`, `doc/` and `xml/` — Pawn includes, examples and documentation.
- `source/compiler/tests/` — compiler regression fixtures and the PowerShell regression script.
- `benchmarks/` — large sample source files and a generator for an 80,000-function workload.

## Optimization work

The current source tree contains several performance-oriented changes, including:

- **Peephole optimization:** candidate patterns are indexed to avoid scanning the full pattern list for every instruction sequence.
- **Stack analysis:** memoization is used for non-recursive call paths, and the traversal is iterative for deep call chains.
- **Unused-function/reference processing:** `reduce_referrers()` uses a queue-based approach rather than repeatedly rescanning the full set.
- **Lexer:** token candidates are narrowed using character-based lookup tables, and character classification uses precomputed tables.
- **Opcode handling:** opcode lookup and reuse avoid repeated string-based searches.
- **AMX output:** output is buffered, including block writes for repeated cells.
- **I/O:** larger buffers are used for source and temporary-file operations.
- **Symbol lookup:** global symbols and repeated referrer lookups use hash-based indexes.

These describe implementation changes in the source. Actual speedups depend on the project, compiler flags, platform and filesystem; use the benchmark fixtures to measure your own workloads.

See [`docs/DEVELOPMENT-STATUS.md`](docs/DEVELOPMENT-STATUS.md) for completed checks and work still required before a stable release.

## Compatibility

APC is designed to preserve Pawn source compatibility and the AMX file format while optimizing internal compiler operations. Do not assume byte-for-byte identical output for every source file, flag combination or compiler version. Compare output and, where possible, run generated AMX files in the target AMX environment before deploying.

The codebase is derived from the Pawn compiler and retains upstream notices. APC is an independent fork and is not an official release of SA-MP, open.mp or ITB CompuPhase.

## Build from source

### Linux (CMake + GCC)

Requirements: CMake, a C compiler and a build tool such as Make or Ninja.

```sh
cmake -S source/compiler -B build/linux \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON
cmake --build build/linux --parallel
ctest --test-dir build/linux --output-on-failure
```

The compiler executable is produced as `build/linux/pawncc` (the shared library and disassembler are built alongside it).

Example:

```sh
build/linux/pawncc -iinclude path/to/script.pwn -d2
```

### Windows (MSVC)

The supplied `build.bat` builds the x86 Windows tools. Install Visual Studio or Build Tools with the **Desktop development with C++** workload and an x86 MSVC toolchain, then run the script from a regular Command Prompt or an x86 Native Tools Command Prompt:

```bat
build.bat
```

The resulting files are placed in `bin\\`:

- `pawncc.exe`
- `pawnc.dll`
- `pawndisasm.exe`

Keep `pawncc.exe` and `pawnc.dll` together. To use APC with Pawno, copy the compiler files to the compiler directory used by your editor, after backing up the existing files.

## Running regression tests

The CMake test suite can be run with:

```sh
ctest --test-dir build/linux --output-on-failure
```

The additional PowerShell regression suite is run separately on Windows:

```powershell
powershell -ExecutionPolicy Bypass -File source/compiler/tests/apc-regressions.ps1 `
  -Compiler .\bin\pawncc.exe `
  -OutputDirectory .\test-output
```

The PowerShell suite generates temporary input files and AMX outputs in the selected output directory.

## Benchmarks

See [`benchmarks/README.md`](benchmarks/README.md). For a fair comparison:

1. Use the same source files, include paths and compiler options.
2. Use the same machine and power profile.
3. Separate cold-cache and warm-cache runs.
4. Perform multiple runs and compare medians.
5. Confirm that both compilers completed successfully and produced valid output.

The historical screenshot in `benchmarks/historical-benchmark.png` is included as project material; it is not a substitute for a reproducible benchmark report.

## Versioning

The project uses `MAJOR.MINOR.PATCH` release labels. The `3.11.0` source line is currently a development snapshot. The Pawn `__Pawn` numeric version follows the upstream compiler's encoded major/minor convention; it is not the full semantic version string.

## License and attribution

The original Pawn license and copyright notices are preserved in [`LICENSE`](LICENSE) and throughout the source files. The project is derived from work by ITB CompuPhase and subsequent Pawn compiler contributors, including the Zeex and pawn-lang projects. Please retain the upstream notices when redistributing modified source versions.

## Contributing

Bug reports should include a minimal `.pwn` reproducer, compiler command line, APC build/version, operating system and the observed output. Performance reports should include the exact input, compiler flags and repeated timing results. See [`CONTRIBUTING.md`](CONTRIBUTING.md).
