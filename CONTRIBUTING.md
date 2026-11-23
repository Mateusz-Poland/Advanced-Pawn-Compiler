# Contributing

## Bug reports

Include:

- A minimal Pawn source file that reproduces the problem.
- The exact compiler command and flags.
- APC version and operating system.
- Expected and actual diagnostics or output.
- Whether the issue reproduces with an upstream Pawn compiler.

Do not attach proprietary gamemode sources. Reduce the issue to a small standalone example.

## Performance reports

Use identical input files, include paths and flags. Run multiple repetitions, report the median, and state whether the filesystem cache was cold or warm. Include output-file sizes and confirm that compilation succeeded.

## Code changes

Keep changes focused. Add or update regression tests for behavior changes. Do not claim a fix or performance improvement without a reproducible test or benchmark.
