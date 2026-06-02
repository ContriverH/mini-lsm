# mini-lsm — Claude Code Conventions

## What this project is
A persistent key-value store in C++17, inspired by LevelDB's storage engine.
Teaching implementation — deliberately reduced scope. See DESIGN.md for all
architecture decisions.

## Hard guardrails — do NOT add without explicit approval
- Compaction of any kind
- Bloom filters
- Block cache
- Concurrent writes / multithreading
- Snapshots, transactions, MVCC
- Tombstone GC
- C++20 features (modules, concepts, ranges)
- Bazel or any build system other than CMake

These are in the Non-Goals section of DESIGN.md for documented reasons.
If you think one is needed, surface it as a question — don't implement it.

## Code conventions
- C++17 standard. Compile with -std=c++17.
- Google C++ style guide as baseline (naming, bracing, include order).
- No exceptions in the storage path — return status codes or booleans.
- No smart pointers where raw pointers + clear ownership suffice.
- Header files: .h extension. Source files: .cpp extension.
- All public methods must have unit tests before the feature is considered done.

## Commit discipline
- Small, atomic commits. One logical change per commit.
- Commit message format: [area] verb-phrase
  e.g., [memtable] implement Put/Get/Delete, [wal] add CRC32 verification
- Do NOT push without explicit approval.

## Design decisions are locked
DESIGN.md sections 1-6 are locked. If implementation reveals a design
problem, surface it as a question. Do not silently deviate from the
design — every decision was made deliberately.

## Journey logging
After any non-trivial milestone, append a one-line entry to
~/google-prep/00-strategy/journey.md. Cat first, propose diff, wait
for approval.

## Cross-repo: prep workspace
- The strategic context for this project lives in ../google-prep/ (private prep repo).
- When this project hits a substantive checkpoint — design lock, weekly milestone shipped, scope change, plan revision — append an entry to ../google-prep/00-strategy/journey.md per the protocol in ../google-prep/CLAUDE.md (cat first, propose diff, wait for approval).
- When in doubt about strategic decisions or scope boundaries, check ../google-prep/07-side-project/mini-lsm/DESIGN.md first.