// SPDX-FileCopyrightText: 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include "common/assert.h"
#include "common/multi_level_page_table.h"
#include "common/virtual_buffer.h"

namespace Common {

template <typename BaseAddr>
MultiLevelPageTable<BaseAddr>::MultiLevelPageTable(std::size_t address_space_bits_, std::size_t first_level_bits_, std::size_t page_bits_)
    : address_space_bits{address_space_bits_}
    , first_level_bits{first_level_bits_}
    , page_bits{page_bits_}
{
    if (page_bits == 0) {
        return;
    }
    first_level_shift = address_space_bits - first_level_bits;
    first_level_chunk_size = (1ULL << (first_level_shift - page_bits)) * sizeof(BaseAddr);
    alloc_size = (1ULL << (address_space_bits - page_bits)) * sizeof(BaseAddr);
    std::size_t first_level_size = 1ULL << first_level_bits;
    first_level_map.resize(first_level_size, nullptr);
#if defined(__PROSPERO__)
    // Anonymous mmap is backed by flexible memory on Prospero. Reserving the
    // complete GPU page table here would therefore commit up to 1 GiB before
    // the guest maps its first page. Allocate first-level chunks lazily
    // instead; operator[] translates the logical flat index below.
#elif defined(_WIN32)
    void* base{VirtualAlloc(nullptr, alloc_size, MEM_RESERVE, PAGE_READWRITE)};
#else
    void* base{mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)};
    if (base == MAP_FAILED)
        base = nullptr;
#endif
#if !defined(__PROSPERO__)
    ASSERT(base);
    base_ptr = reinterpret_cast<BaseAddr*>(base);
#endif
}

template <typename BaseAddr>
MultiLevelPageTable<BaseAddr>::~MultiLevelPageTable() noexcept {
    Release();
}

template <typename BaseAddr>
void MultiLevelPageTable<BaseAddr>::Release() noexcept {
#if defined(__PROSPERO__)
    for (void*& chunk : first_level_map) {
        if (!chunk) {
            continue;
        }
        FreeMemoryPages(chunk, first_level_chunk_size);
        chunk = nullptr;
    }
    base_ptr = nullptr;
#else
    if (!base_ptr) {
        return;
    }
#ifdef _WIN32
    ASSERT(VirtualFree(base_ptr, 0, MEM_RELEASE));
#else
    ASSERT(munmap(base_ptr, alloc_size) == 0);
#endif
    base_ptr = nullptr;
#endif
}

template <typename BaseAddr>
void MultiLevelPageTable<BaseAddr>::ReserveRange(u64 start, std::size_t size) {
    if (size == 0) {
        return;
    }
    if (start > std::numeric_limits<u64>::max() - (size - 1)) [[unlikely]] {
        UNREACHABLE_MSG("multi-level page-table range overflow: start={:#x} size={:#x}", start,
                        size);
    }
    const u64 new_start = start >> first_level_shift;
    const u64 new_end = (start + size - 1) >> first_level_shift;
    if (new_end >= first_level_map.size()) [[unlikely]] {
        UNREACHABLE_MSG("multi-level page-table range is out of bounds: end={} levels={}",
                        new_end, first_level_map.size());
    }
    for (u64 i = new_start; i <= new_end; i++)
        if (!first_level_map[i])
            AllocateLevel(i);
}

template <typename BaseAddr>
void MultiLevelPageTable<BaseAddr>::AllocateLevel(u64 index) {
    if (index >= first_level_map.size()) [[unlikely]] {
        UNREACHABLE_MSG("multi-level page-table level is out of bounds: index={} levels={}",
                        index, first_level_map.size());
    }
#if defined(__PROSPERO__)
    void* base = AllocateMemoryPages(first_level_chunk_size);
    if (!base) [[unlikely]] {
        UNREACHABLE_MSG("Prospero multi-level page-table chunk allocation failed: size={:#x}",
                        first_level_chunk_size);
    }
    std::memset(base, 0, first_level_chunk_size);
#else
    void* ptr = reinterpret_cast<char *>(base_ptr) + index * first_level_chunk_size;
#ifdef _WIN32
    void* base = VirtualAlloc(ptr, first_level_chunk_size, MEM_COMMIT, PAGE_READWRITE);
    ASSERT(base);
#else
    void* base = ptr;
#endif
#endif
    first_level_map[index] = base;
}

template <typename BaseAddr>
const BaseAddr& MultiLevelPageTable<BaseAddr>::EntryAt(std::size_t index) const {
#if defined(__PROSPERO__)
    static const BaseAddr zero{};
    const std::size_t entries_per_level = first_level_chunk_size / sizeof(BaseAddr);
    const std::size_t level = index / entries_per_level;
    if (level >= first_level_map.size()) [[unlikely]] {
        UNREACHABLE_MSG("multi-level page-table read is out of bounds: index={} level={}", index,
                        level);
    }
    const auto* entries = static_cast<const BaseAddr*>(first_level_map[level]);
    return entries ? entries[index % entries_per_level] : zero;
#else
    return base_ptr[index];
#endif
}

template <typename BaseAddr>
BaseAddr& MultiLevelPageTable<BaseAddr>::EntryAt(std::size_t index) {
#if defined(__PROSPERO__)
    const std::size_t entries_per_level = first_level_chunk_size / sizeof(BaseAddr);
    const std::size_t level = index / entries_per_level;
    if (level >= first_level_map.size()) [[unlikely]] {
        UNREACHABLE_MSG("multi-level page-table write is out of bounds: index={} level={}", index,
                        level);
    }
    if (!first_level_map[level]) {
        AllocateLevel(level);
    }
    auto* entries = static_cast<BaseAddr*>(first_level_map[level]);
    return entries[index % entries_per_level];
#else
    return base_ptr[index];
#endif
}

} // namespace Common
