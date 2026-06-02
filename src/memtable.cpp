#include "memtable.h"

namespace mini_lsm {

MemTable::MemTable(size_t flush_threshold)
    : flush_threshold_(flush_threshold) {}

void MemTable::Put(const std::string& key, const std::string& value) {}

void MemTable::Delete(const std::string& key) {}

bool MemTable::Get(const std::string& key, std::string* value,
                   bool* found_tombstone) const {
  return false;
}

size_t MemTable::ApproximateMemoryUsage() const { return 0; }

bool MemTable::ShouldFlush() const { return false; }

std::vector<MemTable::Entry> MemTable::GetSortedEntries() const { return {}; }

}  // namespace mini_lsm
