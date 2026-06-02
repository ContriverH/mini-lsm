# mini-lsm

A persistent key-value store in C++17, inspired by LevelDB's storage engine.
Scope deliberately reduced to the durable write path: in-memory memtable,
write-ahead log, and immutable SSTables with a sparse index.

See [DESIGN.md](DESIGN.md) for the full design document — architecture
decisions, record formats, and non-goals with rationale.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## References

### LSM-tree foundations

- [LevelDB design doc](https://github.com/google/leveldb/blob/main/doc/index.md) — high-level architecture and API
- [LevelDB implementation notes](https://github.com/google/leveldev/blob/main/doc/impl.md) — internals: compaction, recovery, file layout
- [LevelDB table format spec](https://github.com/google/leveldb/blob/main/doc/table_format.md) — SSTable on-disk format

### Tutorial reference

- [mini-lsm by Chi (skyzh)](https://skyzh.github.io/mini-lsm/) — guided LSM-tree build in Rust; architecture translates to C++
- [Week 1 Day 4: SSTable format](https://skyzh.github.io/mini-lsm/week1-04-sst.html) — block layout, index, and encoding
