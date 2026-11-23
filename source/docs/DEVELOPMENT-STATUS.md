# Development status

This repository snapshot is a working development tree for APC 3.11.0. It is not a declaration that the compiler is ready for a stable release.

## Validation completed for this packaged tree

- Linux Release configuration and build with CMake/GCC.
- CTest regression suite: 7 of 7 tests passed.
- Compilation of the included large SA-MP-style `big_test.pwn` fixture.
- `__Pawn` numeric value checked against the 3.11 encoding (`779`).

These checks cover only the listed configurations and inputs.

## Work still required before a stable release

- Complete deterministic allocation-failure testing, including allocation failures during global initialization and hash-table growth.
- Audit allocations and cleanup across fatal-error paths that use `longjmp`.
- Verify `duplicatestring()` callers and all allocation-failure return paths.
- Check that referrer-hash growth failure always has a safe fallback and cannot result in an unbounded probe.
- Run the PowerShell regression script on Windows.
- Build and test the x86 MSVC configuration using the supplied `build.bat`.
- Run ASan/UBSan/LSan or equivalent memory diagnostics on the final post-fix source tree.
- Compare output and runtime behavior against the selected upstream Pawn compiler versions on real projects.

Do not use this list as a substitute for testing the current source. Each item must be re-checked after code changes.
