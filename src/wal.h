// WAL = Write-Ahead Log.
//
// The WAL is a crash-safety mechanism. Before any write (Put/Delete) is
// applied to the in-memory MemTable, it's first written to the WAL file
// on disk. If the process crashes, we replay the WAL on restart to
// reconstruct the MemTable.
//
// Think of it like a database transaction log, or like Redis's AOF
// (Append-Only File). The WAL is append-only — we never modify existing
// data, only add new records at the end.
//
// Each record on disk looks like:
//   [crc32: 4 bytes][type: 1 byte][key_len: 4 bytes][key][value_len: 4 bytes][value]
//
// The CRC32 checksum lets us detect corruption — if the process crashed
// mid-write, the CRC won't match and we know to stop replaying there.

#ifndef MINI_LSM_WAL_H_
#define MINI_LSM_WAL_H_

#include <cstdint>
#include <string>
#include <vector>

// We include memtable.h because WAL uses RecordType (kValue/kDeletion)
// and MemTable (for replay). This is like importing types in TypeScript:
//   import { RecordType, MemTable } from './memtable';
#include "memtable.h"

namespace mini_lsm {

class WAL {
 public:
  // Opens (or creates) a WAL file at the given path.
  // Returns false on failure (e.g., permission denied, disk full).
  //
  // Internally, this calls the POSIX open() syscall, which returns a
  // "file descriptor" (fd) — a small integer that the OS uses to identify
  // an open file. In Node.js, fs.openSync() wraps the same syscall.
  bool Open(const std::string& path);

  // Appends a single Put or Delete record to the WAL file, then calls
  // fsync() to ensure the data is physically written to disk.
  //
  // fsync() is critical: without it, the OS may buffer the write in RAM.
  // If the machine loses power before the buffer is flushed, the data is
  // lost — even though write() returned success. fsync() forces the OS
  // to flush to the physical disk. This is the same concept as
  // fs.fsyncSync(fd) in Node.js.
  //
  // Returns false if the write or fsync fails.
  bool Append(RecordType type, const std::string& key,
              const std::string& value);

  // Replays all valid records from the WAL file into a MemTable.
  // Reads records one by one, verifies each CRC32, and calls Put/Delete
  // on the memtable. Stops at the first record with a bad CRC (partial write).
  //
  // "MemTable* memtable" = pointer to a MemTable (not a reference) because:
  //   1. We will modify it (calling Put/Delete on it)
  //   2. Pointers make the "I will modify this" intent explicit at the call site:
  //      wal.Replay(&memtable)  ← the & makes it clear memtable will be changed
  //
  // "const" at the end = Replay doesn't modify the WAL itself, only reads it.
  bool Replay(MemTable* memtable) const;

  // Closes the file descriptor. After this, Append() would fail.
  // In C++, we typically do this in the destructor too (RAII pattern),
  // but an explicit Close() lets callers control timing.
  void Close();

  // Returns the file path. "const std::string&" = returns a reference
  // to our internal string without copying it. The caller can read it
  // but can't modify it (because const).
  const std::string& Path() const;

 private:
  std::string path_;
  // File descriptor. -1 means "not open". This is a raw integer that the
  // OS kernel uses to track open files. When we call open(), the OS gives
  // us a number (e.g., 3), and we pass that number to write(), read(),
  // fsync(), close(). It's the lowest level of file I/O.
  int fd_ = -1;
};

}  // namespace mini_lsm

#endif  // MINI_LSM_WAL_H_
