// SSTable = Sorted String Table.
//
// An SSTable is an immutable, sorted file on disk. When the MemTable gets
// too large (~4MB), we "flush" it — write all its entries sorted by key
// into an SSTable file. Once written, an SSTable is NEVER modified.
//
// File layout on disk:
//   [Data Block 0][Data Block 1]...[Data Block N][Index Block][Footer]
//
// - Data Blocks: contain the actual key-value records, grouped into ~4KB
//   chunks. 4KB matches the OS page size — the unit of disk I/O.
// - Index Block: a small table at the end that maps each data block's
//   first key to its offset and size. This lets us binary search for a key
//   without scanning the whole file.
// - Footer: the last 16 bytes. Contains a magic number (to verify the file
//   is a valid SSTable) and the position of the index block.
//
// Reading a key from an SSTable:
//   1. Read the footer (last 16 bytes) to find where the index is
//   2. Read the index block to find which data block might contain the key
//   3. Read that one data block and scan it for the key
//
// This is similar to how a book's table of contents works — you don't read
// every page, you check the index first.

#ifndef MINI_LSM_SSTABLE_H_
#define MINI_LSM_SSTABLE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "memtable.h"

namespace mini_lsm {

// Magic number: the bytes 0x6D 0x6C 0x73 0x6D spell "mlsm" in ASCII.
// We write this at the end of every SSTable file. When opening a file,
// we check for this magic number to verify it's actually an SSTable
// and not some random file. Many binary formats do this (ZIP files
// start with "PK", PDF files start with "%PDF", etc.).
//
// "static" here means this constant is local to each translation unit
// that includes this header (avoids linker "multiple definition" errors).
// "constexpr" means the value is computed at compile time.
static constexpr uint32_t kSSTableMagic = 0x6D6C736D;

// Target size for each data block: 4096 bytes = 4KB.
// We keep adding records to a block until it exceeds this size, then
// start a new block. 4KB matches the typical OS page size, which is
// the smallest unit the OS reads from disk. Reading 1 byte or 4KB
// costs roughly the same in terms of disk I/O.
static constexpr size_t kTargetBlockSize = 4096;

// The Footer is a fixed-size 16-byte structure at the very end of the file.
// Fixed size is important — we can always read the last 16 bytes of any
// SSTable to find the footer, regardless of how big the file is.
//
// Memory layout (each field is packed tightly):
//   bytes 0-7:   index_offset (uint64_t, 8 bytes)
//   bytes 8-11:  index_size   (uint32_t, 4 bytes)
//   bytes 12-15: magic        (uint32_t, 4 bytes)
struct Footer {
  uint64_t index_offset;  // Byte position where the index block starts.
  uint32_t index_size;    // Size of the index block in bytes.
  uint32_t magic;         // Must equal kSSTableMagic (0x6D6C736D).
};

// One entry in the index block. Each data block gets one IndexEntry.
// The index is a list of these, telling us: "data block at offset X,
// of size Y, starts with key Z."
struct IndexEntry {
  std::string first_key;    // The first (smallest) key in this data block.
  uint64_t block_offset;    // Byte offset of this data block in the file.
  uint32_t block_size;      // Size of this data block in bytes.
};

// SSTableWriter: takes sorted entries from a MemTable flush and writes
// them to a new SSTable file on disk.
class SSTableWriter {
 public:
  // "static" method = called on the class, not an instance.
  //   SSTableWriter::Write(path, entries);  // not: writer.Write(...)
  // Like TypeScript's static methods. We use static here because the
  // writer doesn't need any state between calls — it's a one-shot operation.
  //
  // "const std::vector<MemTable::Entry>&" = a const reference to a vector
  // of (key, value) pairs. We borrow the vector without copying it.
  static bool Write(const std::string& path,
                    const std::vector<MemTable::Entry>& entries);
};

// SSTableReader: opens an existing SSTable file and provides key lookups.
// On Open(), it reads the footer and index into memory. The actual data
// blocks are only read on demand during Get() — this keeps memory usage low.
class SSTableReader {
 public:
  // Opens the file, reads footer, validates magic, loads index into memory.
  // Returns false if the file is missing, corrupt, or has wrong magic.
  bool Open(const std::string& path);

  // Point lookup: searches the index to find the right data block,
  // reads that block, scans for the key.
  // Same pointer-based output pattern as MemTable::Get().
  bool Get(const std::string& key, std::string* value,
           bool* found_tombstone) const;

  // Utility: checks if a file's footer has the correct magic number.
  // Used during recovery to distinguish complete SSTables from partial ones.
  // "static" because it doesn't need a fully-opened SSTableReader.
  static bool HasValidFooter(const std::string& path);

  const std::string& Path() const;

 private:
  std::string path_;
  Footer footer_;                  // Cached footer (read once on Open).
  std::vector<IndexEntry> index_;  // Cached index (read once on Open).
  // Note: we do NOT cache data blocks — they're read from disk on each Get().
  // This is intentional: caching is a non-goal for v1 (see DESIGN.md).
};

}  // namespace mini_lsm

#endif  // MINI_LSM_SSTABLE_H_
