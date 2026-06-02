# mini-lsm — Design Document

## 1. Problem Statement

LSM-trees are the storage engine behind LevelDB, RocksDB, Cassandra, and most
modern write-optimized databases. Understanding them at the implementation level
— not just the whiteboard level — is the difference between being able to name
the data structure and being able to reason about compaction tradeoffs in a
system design round because you've built the layer underneath.

mini-lsm is a persistent key-value store in C++17, inspired by LevelDB's
storage engine. The scope is deliberately reduced to the durable write path:
in-memory memtable, write-ahead log, and immutable SSTables with a sparse
index. Compaction, bloom filters, concurrency, and MVCC are explicitly out
of v1 — see Non-Goals.

The goal is a working, crash-recoverable store that a reader can understand
end-to-end in one sitting and that the author can defend in a 45-minute
system design round.

## Non-Goals (v1)

These are not deferred — they are deliberately excluded to keep v1 honest:

- **Compaction.** No merging of SSTables. L0 will grow unbounded. This is
  acceptable for a teaching implementation; a production store would need
  size-tiered or leveled compaction to bound read amplification.
- **Bloom filters.** Without compaction producing many overlapping SSTables,
  the read amplification that bloom filters mitigate is manageable.
- **Block cache.** Reads go to disk every time. No in-memory SSTable block
  caching.
- **Concurrent writes.** Single-threaded. No write batching, no mutex on the
  memtable.
- **Snapshots / transactions / MVCC.** No point-in-time reads, no isolation
  levels.
- **Tombstone GC.** Tombstones are written and honored on reads, but never
  garbage-collected (that requires compaction).

---

## 2. Data Model

An LSM-tree's data model is deceptively simple — keys and values are byte
strings, ordered lexicographically. The subtlety is in how deletes are
represented: since SSTables are immutable, you can't remove a key in place.
Instead, you write a tombstone record that the read path interprets as
"this key is deleted." Getting this encoding right at the data model level
avoids friction in every layer above it.

### Keys

- Type: `std::string` (arbitrary bytes, lexicographic ordering)
- Comparison: `std::string::operator<` (byte-wise lexicographic)
- No pluggable comparator in v1. `std::map<std::string, ...>` gives sorted
  iteration for free.

### Values

- Type: `std::string` (arbitrary bytes)
- No hard API limit on size. v1 assumes values fit in a single SSTable data
  block — no multi-block value spanning. This is a deliberate v1
  simplification; LevelDB supports arbitrarily large values via block
  splitting.

### Record Types

Every record (in the memtable, WAL, and SSTable) carries a 1-byte type tag:

| Tag    | Meaning         | Payload        |
|--------|-----------------|----------------|
| `0x01` | Value (Put)     | key + value    |
| `0x00` | Deletion        | key only       |

Deletion records carry no value payload. The read path treats a `0x00`
record as "key does not exist" — returning NotFound even if an older value
record exists in a lower SSTable.

### Ordering

- Within the memtable: sorted by key (std::map invariant).
- Within an SSTable: sorted by key at flush time.
- Across SSTables: read path scans newest-to-oldest. First match wins.
  A tombstone in a newer SSTable shadows a value in an older one.

---

## 3. Memtable

The memtable is the write buffer — every Put and Delete goes here first.
It needs to stay sorted (for efficient SSTable flush) and answer point
lookups fast. For v1, `std::map` gives us O(log n) on both for free. A
skiplist would be the production choice (better cache behavior, lock-free
reads possible), but `std::map` is correct, simple, and not the bottleneck
in a single-threaded store.

### Interface

```cpp
class MemTable {
public:
    void Put(const std::string& key, const std::string& value);
    void Delete(const std::string& key);

    // Returns true if key found. If found and record is a tombstone,
    // sets *found_tombstone = true and leaves *value unchanged.
    bool Get(const std::string& key, std::string* value,
             bool* found_tombstone) const;

    size_t ApproximateMemoryUsage() const;
    bool ShouldFlush() const;

    // Sorted iterator for SSTable flush.
    using Entry = std::pair<std::string, std::string>;  // key -> value (or empty for tombstone)
    std::vector<Entry> GetSortedEntries() const;
};
```

### Backing Structure

`std::map<std::string, Record>` where `Record` holds the type tag (`0x01`
or `0x00`) and the value (empty for deletions). Sorted iteration via
`begin()`/`end()` gives flush-ready ordering with no extra sort pass.

### Memory Budget

Tracked via a running counter incremented on every Put/Delete:

```
per_entry_cost = key.size() + value.size() + kEntryOverhead
```

`kEntryOverhead` is a fixed constant (32 bytes) accounting for `std::map`
node overhead (tree pointers, color bit, `std::string` object headers).
This is approximate — the goal is bounded memory, not byte-exact
accounting. LevelDB takes the same approach.

The budget is checked **after every Put**. Negligible overhead (one
comparison), no risk of overshoot between checks.

### Flush Trigger

When `ApproximateMemoryUsage() >= flush_threshold_`, the store flushes
the memtable to a new SSTable synchronously, blocking the writer until
the flush completes.

Default threshold: **4MB**, configurable at DB-open time.

**v1 simplification:** A production store (LevelDB, RocksDB) freezes the
full memtable, allocates a new empty one immediately, and flushes the
frozen memtable in a background thread — so writes are only blocked for
the cost of a pointer swap, not the full flush I/O. This requires
concurrency (background flush thread + synchronization), which is
explicitly out of v1 scope.

---

## 4. Write-Ahead Log (WAL)

Every write must be durable before it's acknowledged. The WAL is the
crash-recovery guarantee: if the process dies after a Put returns but
before the memtable flushes to an SSTable, the WAL replays those writes
on startup. Without it, the memtable (an in-memory structure) would lose
all unflushed data on crash.

### Record Format

```
[crc32: 4 bytes][type: 1 byte][key_len: 4 bytes][key][value_len: 4 bytes][value]
```

- **crc32** covers everything after it (type + key_len + key + value_len +
  value). On replay, the CRC is recomputed and compared; mismatch =
  corruption detected, replay stops cleanly. Without this, replay would
  silently consume garbage bytes as valid data.
- **type:** `0x01` (Put) or `0x00` (Delete).
- **key_len / value_len:** 4-byte little-endian unsigned integers.
- Deletion records have `value_len = 0` and no value payload.

### Fsync Policy

`fsync` after every write. This is the strongest durability guarantee:
a crash at any point after `write()` + `fsync()` returns means the
record is on disk.

**v1 simplification:** This is slow — one syscall per write, no batching.
Production stores (LevelDB, RocksDB) batch multiple writes into a single
fsync, or fsync periodically (e.g., every N ms), trading a bounded
durability window for throughput. v1 prioritizes the correctness story
over throughput.

### WAL Lifecycle

One WAL file per memtable. The lifecycle is:

1. New memtable created → new WAL file opened.
2. Writes go to both memtable and WAL.
3. Memtable flush triggered → SSTable written and **fsync'd to disk**.
4. Only after SSTable fsync succeeds → WAL file deleted.

The ordering in step 3-4 is critical: SSTable fsync first, then WAL
delete. Reversing this (delete WAL, then crash before SSTable fsync)
loses data silently.

### Replay

On startup, if a WAL file exists, its records are replayed into a fresh
memtable in order. Replay reads records sequentially; the first CRC
mismatch halts replay (all records after the corrupted one are lost).

**Production would:** LevelDB's 32KB block format with per-block checksums
allows skip-and-resume recovery past a corrupted region. v1 stops at
first corruption; production resumes after it.

---

## 5. SSTable

SSTables (Sorted String Tables) are the on-disk persistence format. When the
memtable hits its flush threshold, its sorted entries are written to a new
immutable SSTable file. Once written, an SSTable is never modified — this
immutability is what makes the crash-safety story simple and what makes
compaction (v2) a merge operation rather than an in-place update.

### File Naming

Monotonically increasing IDs: `000001.sst`, `000002.sst`, etc. At startup,
the next ID is derived via `max(existing IDs) + 1`.

**v1 simplification:** Production uses a manifest file for atomic SSTable
visibility (see v2 candidates). v1's SSTable list is append-only, so
directory listing is sufficient for recovery.

### On-Disk Layout

```
[data block 1]
[data block 2]
...
[data block N]
[index block]
[footer: 16 bytes]
```

### Data Blocks

Each data block contains a sorted run of records:

```
[type: 1 byte][key_len: 4 bytes][key][value_len: 4 bytes][value]
```

Same record encoding as the WAL (section 4), minus the CRC — SSTable
integrity is checked at the block/file level, not per-record.

**Target block size: 4KB.** Matches filesystem page size and OS read-ahead
granularity. A block is "filled" when adding the next record would exceed
the target; actual block sizes vary slightly above 4KB.

**Key encoding: full keys per record.** No prefix compression in v1.
LevelDB uses restart-point prefix compression (~20-30% space savings for
keys with shared prefixes), but it complicates iteration and block-level
binary search. See v2 candidates.

### Index Block

One entry per data block:

```
[first_key_len: 4 bytes][first_key][block_offset: 8 bytes][block_size: 4 bytes]
```

- **first_key:** the first (smallest) key in the data block.
- **block_offset:** byte offset of the data block from the start of file.
- **block_size:** byte length of the data block.

The index is sorted by first_key (inherits ordering from the data blocks).
A point lookup binary-searches the index to find the candidate block, then
scans within the block.

### Footer

Fixed 16 bytes at the end of the file:

```
[index_offset: 8 bytes][index_size: 4 bytes][magic: 4 bytes]
```

- **index_offset:** byte offset of the index block from start of file.
- **index_size:** byte length of the index block.
- **magic:** fixed 4-byte value (e.g., `0x6D6C736D` — "mlsm" in ASCII).
  Sanity-checks file identity before parsing; rejects corrupted or
  non-SSTable files early.

### Read Flow (single SSTable)

1. Seek to `file_size - 16`, read footer.
2. Validate magic number.
3. Read index block at `index_offset` (length `index_size`).
4. Binary search index entries for the last entry whose `first_key <= target_key`.
5. Read the candidate data block at `block_offset` (length `block_size`).
6. Linear scan within the block for exact key match.
7. Return value if found (respecting tombstones), or NotFound.

---

## 6. Read Path + Recovery

The read path and recovery logic are where the design decisions from
sections 2-5 come together. Getting recovery right is the difference
between "toy project" and "I understand crash safety" — and crash
recovery is exactly the kind of thing a system design interviewer probes.

### Read Path

```
Get(key):
  1. Check memtable. If found: return value or NotFound (for tombstone).
  2. For each SSTable, newest ID to oldest:
     a. Read footer (last 16 bytes), verify magic.
     b. Load index block via footer.index_offset.
     c. Binary-search index for block whose first_key <= search_key.
     d. Load that data block, scan for exact key match.
     e. If found: return value or NotFound (for tombstone).
  3. Return NotFound.
```

**Invariant: first match wins, including tombstones.** This is what makes
deletes work across multiple SSTables without compaction. A tombstone in
SSTable 000005 shadows a value for the same key in SSTable 000003 — the
read path never reaches 000003 because it stops at the first match.

### WAL-to-SSTable Naming Correspondence

WAL files are named by the SSTable ID they will become on flush:
`000005.wal` flushes to `000005.sst`. This filename correspondence is
the v1 mechanism for detecting crash states during recovery — no
manifest file or sequence numbers needed.

**Production would:** LevelDB uses monotonic sequence numbers plus a
manifest file for atomic SSTable visibility. The filename correspondence
handles the same crash states with less machinery, at the cost of being
tied to v1's append-only SSTable list. See v2 candidates.

### Crash-State Matrix

The WAL lifecycle (section 4) creates three possible filesystem states
on recovery:

| State | Filesystem                                  | What happened                                    |
|-------|---------------------------------------------|--------------------------------------------------|
| 1     | `.wal` exists, no corresponding `.sst`      | Crashed before flush started or completed         |
| 2     | `.wal` exists, partial `.sst` (bad footer)  | Crashed mid-flush                                 |
| 3     | `.wal` exists, complete `.sst` (valid footer)| Crashed between SSTable fsync and WAL delete     |

State 3 is the subtle one: naive replay would duplicate data into a new
SSTable while the completed one becomes orphaned.

### Recovery Algorithm

```
On startup:
  for each .wal file in directory:
    target_sst = corresponding .sst path (same numeric ID)

    if target_sst exists AND footer is valid (magic check):
      # State 3: SSTable already durable, WAL is redundant
      delete .wal

    elif target_sst exists AND footer invalid/missing:
      # State 2: partial flush, .sst is garbage
      delete partial .sst
      replay .wal into memtable
      if ShouldFlush(): flush to .sst, delete .wal

    else:
      # State 1: no SSTable yet
      replay .wal into memtable
      if ShouldFlush(): flush to .sst, delete .wal

  Build SSTable list from all valid .sst files, sorted by ID.
  Store is open for reads and writes.
```

### Flush-on-Recovery

If WAL replay produces a memtable at or above the flush threshold, the
store flushes immediately before accepting new writes. This maintains the
invariant that the store always opens with a sub-threshold memtable and
exercises the same flush code path used during normal operation.

---

## v2 Candidates / Future Work

These are not commitments to build. They exist as a thinking artifact —
documenting what a production store would need and why v1 doesn't
include them strengthens the "deliberately reduced" framing.

### Compaction (leveled or size-tiered, with tombstone GC)

Without compaction, L0 SSTables accumulate indefinitely: disk usage grows
monotonically, read amplification increases linearly with flush count, and
tombstones are never garbage-collected. Compaction merges overlapping
SSTables, reclaims space from deleted keys, and bounds read amplification.
Not in v1 because it's the single largest complexity surface in an LSM
store — merge scheduling, atomic file swaps, and manifest versioning are
each non-trivial.

### Manifest file (versioned SSTable metadata)

Once compaction creates and deletes SSTables atomically, the store needs a
durable record of which SSTables are live at any point in time. The manifest
is that record — a write-ahead log of SSTable-level metadata changes. Without
compaction, v1's SSTable list is append-only and can be recovered by listing
data files on disk. Compaction breaks that invariant.

### Bloom filters on SSTables

A point lookup currently scans every SSTable from newest to oldest until it
finds a match or exhausts the list. A bloom filter per SSTable lets the read
path skip SSTables that definitely don't contain the key — turning an O(N)
SSTable scan into O(1) expected for non-existent keys. Not in v1 because
without compaction the SSTable count stays manageable (bounded by total data
size / 4MB flush threshold), and the implementation complexity (hash
functions, false-positive tuning, serialization into the SSTable footer)
isn't justified.

### Block cache for hot SSTable reads

Every SSTable read currently goes to disk. A block cache (LRU over SSTable
data blocks) would absorb repeated reads of hot keys without a syscall. Not
in v1 because the single-threaded, non-compacting store doesn't generate
the read traffic patterns where caching matters — and adding a cache before
adding compaction would optimize the wrong bottleneck.

### Concurrent writes (memtable freeze + background flush thread)

v1 blocks the writer for the entire SSTable flush duration. A production
store freezes the current memtable, swaps in a new one (pointer swap, ~ns),
and flushes the frozen memtable in a background thread. This requires
thread-safe memtable access, a flush queue, and synchronization on the
SSTable list. Not in v1 because concurrency bugs in a storage engine are
the kind of bugs that silently corrupt data — getting the single-threaded
path correct first is the right sequencing.

### Restart-point prefix compression for data block keys

Keys within a data block often share long prefixes (e.g., `user:1001`,
`user:1002`). LevelDB stores only the non-shared suffix for most keys,
with periodic "restart points" that store the full key to allow binary
search within a block. Saves ~20-30% on key storage for prefix-heavy
workloads. Not in v1 because full keys simplify block scanning and the
space savings don't justify the iteration complexity for a teaching
implementation.

### MVCC / snapshots (sequence numbers per record)

Adding a monotonically increasing sequence number to each record enables
point-in-time snapshots: a reader holds a sequence number and ignores any
record written after it. This is the foundation of transaction isolation
in LevelDB and RocksDB. Not in v1 because MVCC touches every layer
(memtable ordering, SSTable encoding, compaction's tombstone visibility
rules) and its value is only realized with concurrent readers, which are
out of scope.
