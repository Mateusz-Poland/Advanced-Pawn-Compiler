# Changelog

## 3.11.0 — assembler and include performance

- Index assembly instructions in memory with a bounded four-byte record.
- Reuse the index across segment emission and compact-encoding retry.
- Cache successful and unsuccessful include resolutions within a compilation.
- Profile assembler passes, opcode lookups and include probes.

## 3.11.0 - performance release

- Batch global symbol insertion with stable ordering at list traversal boundaries.
- Index macro prefixes and large tag tables by name.
- Track unused referrer slots and scan only the used range of referrer arrays.
- Store switch cases in a compact sorted buffer and append increasing labels directly.
- Extend regression coverage for out-of-order public symbols, macro redefinition and growth, tag lookup, and switch ranges.

## 3.11.0 - previous development snapshot

- Added performance-oriented indexing and caching in compiler hot paths.
- Improved stack-analysis traversal and memoization for non-recursive call paths.
- Reworked unused-reference processing to use a queue.
- Added large-source benchmark fixtures and an 80,000-function test generator.
- Corrected the encoded Pawn major/minor version value to 779 for 3.11.

