// "Include guard" — prevents this file from being included twice in the same
// compilation unit. Without this, if both db.h and wal.h include memtable.h,
// and some .cpp includes both, the compiler would see duplicate definitions.
// #pragma once is the modern way; the #ifndef pattern below is the traditional
// way. We use both for maximum compatibility.
#ifndef MINI_LSM_MEMTABLE_H_
#define MINI_LSM_MEMTABLE_H_

// Standard library headers — each gives us specific types/functions:
#include <cstdint>   // Fixed-width integers: uint8_t (1 byte), uint32_t (4 bytes), etc.
                     // Unlike JS where all numbers are 64-bit floats, C++ has
                     // precise control over integer sizes — critical for binary formats.
#include <map>       // std::map — a sorted key-value container (red-black tree).
                     // Like JS Map but keys are always in sorted order.
#include <string>    // std::string — a heap-allocated, mutable string.
                     // Unlike JS strings which are immutable, C++ strings can be modified.
#include <utility>   // std::pair — a simple two-element tuple.
                     // Like TypeScript's [string, string] tuple type.
#include <vector>    // std::vector — a dynamic array (like JS Array, but typed).
                     // Contiguous in memory, so cache-friendly and fast to iterate.

// Namespaces prevent name collisions — like ES modules or TypeScript namespaces.
// Everything inside this block is accessed as mini_lsm::MemTable, etc.
// Without this, if another library also had a class called "MemTable",
// the linker wouldn't know which one you meant.
namespace mini_lsm {

// Enum that maps to a single byte (uint8_t) stored on disk.
// kValue = 0x01 means "this record is a Put (key has a value)"
// kDeletion = 0x00 means "this record is a Delete (key was removed)"
//
// Why not just delete the key from the map? Because we need to write this
// to SSTable files. An SSTable is immutable — if key "foo" exists in an
// older SSTable, we need a tombstone in a newer SSTable to indicate it's
// been deleted. Without tombstones, a read would find the old value.
enum RecordType : uint8_t {
  kDeletion = 0x00,
  kValue = 0x01,
};

// A record stored in the memtable. Like a TypeScript interface:
//   interface Record { type: RecordType; value: string; }
//
// struct vs class: In C++, a struct is identical to a class except that
// members are public by default (class members are private by default).
// Convention: use struct for simple data holders, class for objects with
// behavior and invariants.
struct Record {
  RecordType type;
  std::string value;  // Empty string for deletions.
};

// The MemTable: an in-memory sorted key-value store.
//
// This is the "write buffer" of the LSM tree. All writes (Put/Delete) go
// here first. When it gets too large (~4MB), we flush it to an SSTable
// on disk. Using std::map gives us O(log n) insert/lookup AND sorted
// iteration for free — both are needed.
class MemTable {
 public:
  // Constructor. The "explicit" keyword prevents accidental implicit conversions.
  // Without explicit: MemTable mt = 1024; would compile (bad, confusing).
  // With explicit: you must write MemTable mt(1024); (clear intent).
  //
  // The default value "4 * 1024 * 1024" = 4MB. If you don't pass a threshold,
  // you get 4MB. Like a default parameter in TypeScript.
  //
  // size_t = an unsigned integer type guaranteed to hold the size of any object
  // in memory. On 64-bit systems, it's 8 bytes. Used for sizes and counts.
  explicit MemTable(size_t flush_threshold = 4 * 1024 * 1024);

  // Inserts or updates a key-value pair.
  //
  // "const std::string&" = a const reference.
  //   - const: we promise not to modify the caller's string
  //   - & (reference): we borrow it instead of copying it
  //   Without &, C++ would COPY the entire string into a new variable (expensive).
  //   This is the #1 difference from JS — in JS, objects are always passed by
  //   reference automatically. In C++, you must be explicit.
  void Put(const std::string& key, const std::string& value);

  // Inserts a tombstone record for the key (marks it as deleted).
  void Delete(const std::string& key);

  // Looks up a key. Returns true if found (even if it's a tombstone).
  //
  // "std::string* value" = a POINTER to a string.
  //   - Why pointer instead of return value? Because we need to return TWO things:
  //     (1) whether the key was found (the bool return), and
  //     (2) the value itself (written through the pointer).
  //   - In JS, you'd return { found: true, value: "world" }.
  //   - In C++, returning a struct every time is a common pattern too, but
  //     the pointer-out-parameter pattern is traditional in systems code
  //     (it avoids heap allocation and is explicit about mutation).
  //
  // "bool* found_tombstone" = tells the caller whether the key was deleted.
  //   The caller needs this to know: "stop searching SSTables — the key is
  //   definitively deleted" vs "key not found here, keep looking."
  //
  // "const" at the end of the method = this method doesn't modify the MemTable.
  //   The compiler enforces this. Like TypeScript's readonly, but for methods.
  bool Get(const std::string& key, std::string* value,
           bool* found_tombstone) const;

  // Returns an estimate of how much memory this memtable is using.
  // Used to decide when to flush to disk.
  size_t ApproximateMemoryUsage() const;

  // Returns true when memory usage exceeds the flush threshold.
  // The DB layer checks this after every write.
  bool ShouldFlush() const;

  // Returns all entries as sorted (key, value) pairs for flushing to SSTable.
  //
  // "using Entry = std::pair<std::string, std::string>" = a type alias.
  //   Like TypeScript's: type Entry = [string, string];
  //   std::pair is a simple two-field struct with .first and .second.
  using Entry = std::pair<std::string, std::string>;
  std::vector<Entry> GetSortedEntries() const;

 // "private:" = everything below is internal. Other code can't access these
 // directly — only through the public methods above. Like TypeScript's private.
 private:
  // "static constexpr" = a compile-time constant. The compiler replaces every
  // use of kEntryOverhead with the literal value 32. No memory is allocated.
  // Like TypeScript's: const ENTRY_OVERHEAD = 32 as const;
  // but evaluated at compile time, not runtime.
  //
  // This accounts for std::map node overhead (pointers, color bit for the
  // red-black tree) when estimating memory usage.
  static constexpr size_t kEntryOverhead = 32;

  // The sorted map holding all entries. std::map is a balanced BST (red-black
  // tree), so insert and lookup are O(log n). Iteration visits keys in sorted
  // order — exactly what we need for SSTable flush.
  std::map<std::string, Record> entries_;

  // Running tally of approximate memory used. We update this on every
  // Put/Delete rather than computing it from scratch each time.
  size_t memory_usage_ = 0;

  // When memory_usage_ exceeds this, ShouldFlush() returns true.
  size_t flush_threshold_;

  // Naming convention note:
  // Trailing underscore (entries_, memory_usage_) marks private member
  // variables. This is a Google C++ style guide convention — it prevents
  // name collisions with method parameters and makes it visually obvious
  // which variables are instance state.
};

}  // namespace mini_lsm

#endif  // MINI_LSM_MEMTABLE_H_
