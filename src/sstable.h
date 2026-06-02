#ifndef MINI_LSM_SSTABLE_H_
#define MINI_LSM_SSTABLE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "memtable.h"

namespace mini_lsm {

// Magic number: "mlsm" in ASCII.
static constexpr uint32_t kSSTableMagic = 0x6D6C736D;

// Target data block size (section 5: 4KB, matches page size).
static constexpr size_t kTargetBlockSize = 4096;

// Footer: fixed 16 bytes at end of file.
struct Footer {
  uint64_t index_offset;
  uint32_t index_size;
  uint32_t magic;
};

// One entry in the index block.
struct IndexEntry {
  std::string first_key;
  uint64_t block_offset;
  uint32_t block_size;
};

class SSTableWriter {
 public:
  // Writes sorted entries to a new SSTable file.
  // Returns false on failure.
  static bool Write(const std::string& path,
                    const std::vector<MemTable::Entry>& entries);
};

class SSTableReader {
 public:
  // Opens an SSTable file and reads its footer + index.
  // Returns false if file is missing, corrupt, or magic mismatch.
  bool Open(const std::string& path);

  // Point lookup. Returns true if key found.
  // If found and record is a tombstone, sets *found_tombstone = true.
  bool Get(const std::string& key, std::string* value,
           bool* found_tombstone) const;

  // Validates that the footer has the correct magic number.
  static bool HasValidFooter(const std::string& path);

  const std::string& Path() const;

 private:
  std::string path_;
  Footer footer_;
  std::vector<IndexEntry> index_;
};

}  // namespace mini_lsm

#endif  // MINI_LSM_SSTABLE_H_
