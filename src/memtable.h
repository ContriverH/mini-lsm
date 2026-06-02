#ifndef MINI_LSM_MEMTABLE_H_
#define MINI_LSM_MEMTABLE_H_

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace mini_lsm {

// Record type tags (section 2: 0x01 = Put, 0x00 = Delete).
enum RecordType : uint8_t {
  kDeletion = 0x00,
  kValue = 0x01,
};

// Memtable record: type tag + value (empty for deletions).
struct Record {
  RecordType type;
  std::string value;
};

class MemTable {
 public:
  explicit MemTable(size_t flush_threshold = 4 * 1024 * 1024);

  void Put(const std::string& key, const std::string& value);
  void Delete(const std::string& key);

  // Returns true if key found. If found and record is a tombstone,
  // sets *found_tombstone = true and leaves *value unchanged.
  bool Get(const std::string& key, std::string* value,
           bool* found_tombstone) const;

  size_t ApproximateMemoryUsage() const;
  bool ShouldFlush() const;

  // Sorted iterator for SSTable flush.
  using Entry = std::pair<std::string, std::string>;
  std::vector<Entry> GetSortedEntries() const;

 private:
  static constexpr size_t kEntryOverhead = 32;

  std::map<std::string, Record> entries_;
  size_t memory_usage_ = 0;
  size_t flush_threshold_;
};

}  // namespace mini_lsm

#endif  // MINI_LSM_MEMTABLE_H_
