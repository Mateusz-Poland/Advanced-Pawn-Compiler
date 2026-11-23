# Changelog

## 3.11.0 — development snapshot

This is a working development snapshot, not a stable release.

- Added performance-oriented indexing and caching in compiler hot paths.
- Improved stack-analysis traversal and memoization for non-recursive call paths.
- Reworked unused-reference processing to use a queue.
- Added large-source benchmark fixtures and an 80,000-function test generator.
- Corrected the encoded Pawn major/minor version value to `779` for 3.11.

Further fixes and validation are still required before a stable release is declared.
