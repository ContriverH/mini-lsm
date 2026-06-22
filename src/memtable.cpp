// This is the IMPLEMENTATION file for MemTable.
//
// In C++, the header (.h) declares WHAT exists (the interface),
// and the source (.cpp) defines HOW it works (the implementation).
//
// The compiler processes each .cpp file independently (called a
// "translation unit"). It needs the header to know the class layout,
// then compiles this file into an object file (memtable.o).
// Later, the linker combines all .o files into the final binary.

// Include our own header first — this is a Google style convention.
// It ensures the header is self-contained (compiles on its own without
// relying on includes from other files that happen to come before it).
#include "memtable.h"

namespace mini_lsm {

// Constructor implementation.
//
// "MemTable::MemTable" = we're defining the MemTable constructor that
// belongs to the MemTable class. The ClassName:: prefix is needed because
// this definition is outside the class body (which is in the header).
//
// ": flush_threshold_(flush_threshold)" is an INITIALIZER LIST.
// It initializes the member variable BEFORE the constructor body runs.
// This is more efficient than assigning inside the body because it
// constructs the value directly rather than default-constructing then
// overwriting. For simple types like size_t, the difference is tiny,
// but it's good practice and required for const/reference members.
MemTable::MemTable(size_t flush_threshold)
    : flush_threshold_(flush_threshold) {}

// ---- STUB IMPLEMENTATIONS ----
// All methods below are empty placeholders. They compile and link,
// allowing us to build and run tests (which will fail, confirming
// our test catches the missing behavior — TDD "red" phase).

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
