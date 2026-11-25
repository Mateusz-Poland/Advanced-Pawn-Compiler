# Validation status

This release is APC 3.11.0. The package contains x86 Windows binaries built from the accompanying sources.

- Clean MSVC x86 build via build.bat. The compiler driver, DLL and disassembler were exercised.
- CMake Release build with MSVC and Ninja. All seven CTest cases passed.
- The extended Windows PowerShell suite passed, including direct and mutual recursion, a 20,000-call chain, and 32,769 public functions declared out of order.
- Version 3.11.0 and the Pawn numeric version 779 were checked.
- See [PERFORMANCE.ru.md](../PERFORMANCE.ru.md) for the tested workloads, AMX hashes and timings.

Remaining validation includes a Linux build of this revision, sanitizers and deterministic allocation-failure tests, and execution of the AMX files in a SA-MP or open.mp server.
