# Benchmark fixtures

This directory contains optional, large Pawn input files used for local performance and regression checks.

## SA-MP-style large source

- `samp-large-project/gamemodes/big_test.pwn` is the large sample source from the supplied package.
- `samp-large-project/include/` contains the accompanying include set.

Use the empty prefix option `-p` when compiling these SA-MP fixtures from the release directory. This lets the fixture provide the native declarations through a_samp.

Example (Windows):

```bat
bin\pawncc.exe -p -i"benchmarks\samp-large-project\include" "benchmarks\samp-large-project\gamemodes\big_test.pwn" -d2 -obig_test.amx
```

Example (Linux):

```sh
build/linux/pawncc -p -ibenchmarks/samp-large-project/include \
  benchmarks/samp-large-project/gamemodes/big_test.pwn -d2 -obuild/big_test.amx
```

## Generated 80,000-function workload

Run `generators/generate-realistic-gm.bat` from a writable working directory on Windows. It creates `realistic_gm.pwn` with 80,000 function definitions and 80,000 calls. Compile it with the include directory above:

```bat
bin\pawncc.exe -p -i"benchmarks\samp-large-project\include" realistic_gm.pwn -d2
```

The generated workload is synthetic. It is useful for stress testing but does not represent every aspect of a real modular gamemode or include-heavy project.

For meaningful performance comparisons, use the same machine, compiler flags, source files, include paths and output settings; run multiple repetitions and compare medians. Do not compare one cold run against one warm run.
