#ifndef MINI_LSM_DB_H_
#define MINI_LSM_DB_H_

#include <cstdint>
#include <string>
#include <vector>

#include "memtable.h"
#include "sstable.h"
#include "wal.h"

namespace mini_lsm {

struct DBOptions {
  std::string dir;
  size_t flush_threshold = 4 * 1024 * 1024;
};

class DB {
 public:
  // Opens the store at the given directory. Runs recovery if needed.
  static bool Open(const DBOptions& options, DB** db);

  ~DB();

  bool Put(const std::string& key, const std::string& value);
  bool Delete(const std::string& key);

  // Returns true if key found (including tombstone detection).
  // Returns false for NotFound.
  bool Get(const std::string& key, std::string* value) const;

 private:
  DB() = default;

  // Recovery: process WAL files per crash-state matrix (section 6).
  bool Recover();

  // Flushes current memtable to a new SSTable, rotates WAL.
  bool FlushMemTable();

  // Assigns the next monotonically increasing SSTable/WAL ID.
  uint32_t NextFileId();

  DBOptions options_;
  MemTable memtable_;
  WAL wal_;
  std::vector<SSTableReader> sstables_;  // newest-first order
  uint32_t next_file_id_ = 1;
};

}  // namespace mini_lsm

#endif  // MINI_LSM_DB_H_
