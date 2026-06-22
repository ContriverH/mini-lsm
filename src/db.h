// DB: the top-level database class that ties everything together.
//
// This is the public API that users of our library interact with:
//   DB* db;
//   DB::Open(options, &db);
//   db->Put("name", "himanshu");
//   db->Get("name", &value);
//
// Internally, it orchestrates:
//   1. MemTable — in-memory buffer for recent writes
//   2. WAL — crash-safety log for the MemTable
//   3. SSTables — sorted files on disk for older data
//
// Write path:  Write to WAL -> Write to MemTable -> (if full) Flush to SSTable
// Read path:   Check MemTable -> Check SSTables newest-first -> Not Found

#ifndef MINI_LSM_DB_H_
#define MINI_LSM_DB_H_

#include <cstdint>
#include <string>
#include <vector>

// Include all component headers — DB owns and coordinates these.
#include "memtable.h"
#include "sstable.h"
#include "wal.h"

namespace mini_lsm {

// Configuration for opening a database. Using a struct for options is a
// common C++ pattern — it's cleaner than passing many parameters and
// makes it easy to add new options without changing the function signature.
// Like TypeScript's: interface DBOptions { dir: string; flushThreshold?: number; }
struct DBOptions {
  std::string dir;  // Directory where WAL and SSTable files are stored.
  size_t flush_threshold = 4 * 1024 * 1024;  // 4MB default.
};

class DB {
 public:
  // Factory method: creates and opens a database.
  //
  // "static bool Open(const DBOptions& options, DB** db)"
  //
  // Why not just use a constructor? Because constructors in C++ can't
  // return error codes — they either succeed or throw an exception.
  // Our project avoids exceptions in the storage path (see CLAUDE.md),
  // so we use a static factory method that returns bool for success/failure.
  //
  // "DB** db" = pointer to a pointer. This is the "out parameter" pattern.
  // The caller passes in a pointer variable, and we set it to point to
  // the newly created DB object:
  //
  //   DB* db = nullptr;        // caller's pointer, initially null
  //   DB::Open(opts, &db);     // we create a new DB and set db to point to it
  //   db->Put("key", "val");   // caller uses it through the pointer
  //
  // In TypeScript, you'd just return the object or throw:
  //   const db = DB.open(opts);  // throws on failure
  // But in C++, the out-parameter pattern is idiomatic for failable construction.
  static bool Open(const DBOptions& options, DB** db);

  // Destructor: called automatically when the DB object is destroyed.
  // In JS/TS, the garbage collector handles cleanup. In C++, YOU control
  // when objects die, and the destructor runs at that exact moment.
  //
  // Our destructor will:
  //   - Flush the current memtable to disk (don't lose unflushed data)
  //   - Close the WAL file descriptor
  //   - Any other cleanup
  //
  // This is the RAII pattern (Resource Acquisition Is Initialization):
  //   - Acquire resources in the constructor (open files, allocate memory)
  //   - Release resources in the destructor (close files, free memory)
  //   The resource's lifetime is tied to the object's lifetime. This is
  //   arguably C++'s most powerful concept — no GC needed, deterministic cleanup.
  ~DB();

  // Public API — same patterns as MemTable but returns bool for I/O errors.
  bool Put(const std::string& key, const std::string& value);
  bool Delete(const std::string& key);

  // Get checks memtable first, then SSTables in newest-first order.
  // Returns false for "key not found" (not an error — just doesn't exist).
  bool Get(const std::string& key, std::string* value) const;

 private:
  // "DB() = default;" = the compiler generates a default constructor.
  // It's private so only the static Open() method can create DB instances.
  // This enforces that all DBs are created through Open() — preventing
  // half-initialized objects.
  DB() = default;

  // Crash recovery: reads existing WAL and SSTable files on startup.
  // Handles three crash scenarios documented in DESIGN.md section 6:
  //   1. Clean shutdown — WAL is empty, SSTables are complete
  //   2. Crash after WAL write but before flush — replay WAL
  //   3. Crash during flush — partial SSTable, replay WAL
  bool Recover();

  // Flushes the current memtable to a new SSTable file, then creates
  // a fresh empty memtable and new WAL for future writes.
  bool FlushMemTable();

  // Returns the next unused file ID for naming WAL/SSTable files.
  // Files are named like: 000001.wal, 000001.sst, 000002.wal, etc.
  uint32_t NextFileId();

  // --- Member variables (instance state) ---

  DBOptions options_;

  // The current active memtable. Declared directly (not as a pointer),
  // so its lifetime is tied to the DB object's lifetime — it's constructed
  // when DB is constructed and destroyed when DB is destroyed.
  MemTable memtable_;

  // The current active WAL, corresponding to the current memtable.
  WAL wal_;

  // All SSTables on disk, kept in newest-first order so reads check
  // the most recent data first. We store the Reader objects (with their
  // cached index) — not the actual data blocks.
  std::vector<SSTableReader> sstables_;

  // Monotonically increasing counter for generating unique file names.
  uint32_t next_file_id_ = 1;
};

}  // namespace mini_lsm

#endif  // MINI_LSM_DB_H_
