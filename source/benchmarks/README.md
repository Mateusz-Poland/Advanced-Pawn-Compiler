# Benchmark fixtures

This directory contains optional, large Pawn input files used for local performance and regression checks.

## SA-MP-style large source

- `samp-large-project/gamemodes/big_test.pwn` is the large sample source from the supplied package.
- `samp-large-project/include/` contains the accompanying include set.

Example (Windows):

```bat
bin\pawncc.exe -i"benchmarks\samp-large-project\include" "benchmarks\samp-large-project\gamemodes\big_test.pwn" -d2 -obuild\big_test.amx
```

Example (Linux):

```sh
build/linux/pawncc -ibenchmarks/samp-large-project/include \
  benchmarks/samp-large-project/gamemodes/big_test.pwn -d2 -obuild/big_test.amx
```

## Generated 80,000-function workload

Run `generators/generate-realistic-gm.bat` from a writable working directory on Windows. It creates `realistic_gm.pwn` with 80,000 function definitions and 80,000 calls. Compile it with the include directory above:

```bat
bin\pawncc.exe -i"benchmarks\samp-large-project\include" realistic_gm.pwn -d2
```

The generated workload is synthetic. It is useful for stress testing but does not represent every aspect of a real modular gamemode or include-heavy project.

For meaningful performance comparisons, use the same machine, compiler flags, source files, include paths and output settings; run multiple repetitions and compare medians. Do not compare one cold run against one warm run.
