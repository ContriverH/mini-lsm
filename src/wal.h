#ifndef MINI_LSM_WAL_H_
#define MINI_LSM_WAL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "memtable.h"

namespace mini_lsm {

class WAL {
 public:
  // Opens (or creates) a WAL file at the given path.
  // Returns false on failure.
  bool Open(const std::string& path);

  // Appends a record to the WAL and fsyncs.
  // Format: [crc32: 4][type: 1][key_len: 4][key][value_len: 4][value]
  bool Append(RecordType type, const std::string& key,
              const std::string& value);

  // Replays all valid records into the given memtable.
  // Stops at first CRC mismatch.
  bool Replay(MemTable* memtable) const;

  // Closes the WAL file descriptor.
  void Close();

  const std::string& Path() const;

 private:
  std::string path_;
  int fd_ = -1;
};

}  // namespace mini_lsm

#endif  // MINI_LSM_WAL_H_
