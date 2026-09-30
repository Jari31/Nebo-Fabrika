
#pragma once

#include <cstddef>
#include <cstdlib>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <memoryapi.h>
#include <winnt.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "DebugHelpers.hpp"
#include "cerrno"
#include "iostream"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

namespace JSlang
{
using Symbol = uint32_t;

struct ArenaAllocator
{
    uint8_t *base              = nullptr;
    size_t   reserved_capacity = 0;
    size_t   committed_size    = 0;
    size_t   current_offset    = 0;

    bool Initialize(size_t DefaultMemoryChunkSize = 4ULL * 1024 * 1024 * 1024)
    {
        reserved_capacity = DefaultMemoryChunkSize;

#if defined(_WIN32)
        base = static_cast<uint8_t *>(
            VirtualAlloc(NULL, reserved_capacity, MEM_RESERVE, PAGE_NOACCESS)); // NOLINT
#else
        base = static_cast<uint8_t *>(
            mmap(NULL, reserved_capacity, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0)); // NOLINT
        if (base == MAP_FAILED) base = nullptr;
#endif

        return base != nullptr;
    }

    [[noreturn]] [[gnu::cold]] void PANIC_OUT_OF_MEMORY(size_t RequestedBytes) // NOLINT
    {
        std::cerr << "\n[FATAL ERROR] : ArenaAllocator out of memory.\n"
                  << "Failed to allocate " << RequestedBytes << " bytes.\n"
                  << "Reserved 4GB address space exhausted.\n";

        std::clog.flush();
        std::cerr.flush();

        DebugHelpers::BuiltinTrap();

        std::abort();
    }

    bool CommitPages(size_t BytesToCommit)
    {
        if (BytesToCommit <= committed_size)
        {
            return true;
        }
        if (BytesToCommit > reserved_capacity)
        {
            PANIC_OUT_OF_MEMORY(BytesToCommit);
            return false;
        }

        size_t size_needed = BytesToCommit - committed_size;

#if defined(_WIN32)
        void *result = VirtualAlloc(base + committed_size, size_needed, MEM_COMMIT, PAGE_READWRITE);
        if (result == nullptr)
        {
            PANIC_OUT_OF_MEMORY(BytesToCommit);
            return false;
        }
#else
        if (mprotect(base + committed_size, size_needed, PROT_READ | PROT_WRITE) != 0)
        {
            PANIC_OUT_OF_MEMORY(BytesToCommit);
            return false;
        }
#endif

        committed_size = BytesToCommit;
        return true;
    }

    void ReleaseMemory()
    {
        if (base == nullptr)
        {
            return;
        }
#if defined(_WIN32)
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, reserved_capacity);
#endif
        base           = nullptr;
        committed_size = 0;
    }

    ArenaAllocator() = default;

    ~ArenaAllocator() { ReleaseMemory(); }

    ArenaAllocator(const ArenaAllocator &)            = delete;
    ArenaAllocator &operator=(const ArenaAllocator &) = delete;

    void check_if_target_commit_size_out_of_bounds(size_t TargetSize)
    {
        if (TargetSize > reserved_capacity) [[unlikely]]
        {
            PANIC_OUT_OF_MEMORY(TargetSize);
        }
    };

    static size_t calculate_target_commit(size_t Base) { return (Base + 0xFFFF) & ~0xFFFF; }

    uint8_t *AllocateBlockOfSize(size_t Size, size_t Alignment = 1)
    {
        auto *current_pointer = base + current_offset;

        auto   pointer_value   = reinterpret_cast<std::uintptr_t>(current_pointer);
        size_t aligned_pointer = (pointer_value + Alignment - 1) & ~(Alignment - 1);
        size_t padding         = aligned_pointer - pointer_value;

        size_t new_offset = current_offset + padding + Size;

        check_if_target_commit_size_out_of_bounds(new_offset);

        if ((new_offset) > committed_size) [[unlikely]]
        {
            size_t target_commit = calculate_target_commit(new_offset);

            check_if_target_commit_size_out_of_bounds(target_commit);

            CommitPages(target_commit);
            return AllocateBlockOfSize(Size, Alignment);
        }

        current_offset += new_offset;
        return current_pointer + padding;
    }
    template <typename Type, typename... ArgumentTypes>
    uint32_t Allocate(ArgumentTypes &&...Arguments)
    {
        size_t alignment = alignof(Type);
        size_t size      = sizeof(Type);

        auto   current_pointer = reinterpret_cast<size_t>(base + current_offset);
        size_t aligned_pointer = (current_pointer + alignment - 1) & ~(alignment - 1);
        size_t padding         = aligned_pointer - current_pointer;

        size_t new_offset = current_pointer + padding + size;

        check_if_target_commit_size_out_of_bounds(new_offset);

        if (new_offset > committed_size) [[unlikely]]
        {
            size_t target_commit = calculate_target_commit(new_offset);

            check_if_target_commit_size_out_of_bounds(target_commit);

            if (!CommitPages(target_commit))
            {
                PANIC_OUT_OF_MEMORY(target_commit);
            };
        }

        current_offset += new_offset;
        Type *result = reinterpret_cast<Type *>(aligned_pointer);

        new (result) Type(std::forward<ArgumentTypes>(Arguments)...);

        return static_cast<uint32_t>(aligned_pointer - reinterpret_cast<size_t>(base));
    }
    template <typename Type> std::span<Type> AllocateArray(size_t Count)
    {
        if (Count == 0)
        {
            return {};
        }

        size_t alignment = alignof(Type);
        size_t size      = sizeof(Type) * Count;

        auto   current_pointer = reinterpret_cast<size_t>(base + current_offset);
        size_t aligned_pointer = (current_pointer + alignment - 1) & ~(alignment - 1);
        size_t padding         = aligned_pointer - current_pointer;

        if (current_offset + padding + size > reserved_capacity)
        {
            PANIC_OUT_OF_MEMORY(size);
        }

        current_offset += padding + size;
        auto *resulting_array = reinterpret_cast<Type *>(aligned_pointer);

        // if constexpr (!std::is_trivially_constructible_v<Type>)
        // {
        for (size_t i = 0; i < Count; i++)
        {
            new (&resulting_array[i]) Type();
        }
        // }

        return std::span<Type>(resulting_array, Count);
    }

    template <typename Type, bool CheckNull = false> Type *ResolveTypePtrFromHandle(uint32_t Handle)
    {
        if constexpr (CheckNull)
        {
            if (Handle == 0)
            {
                return nullptr;
            }
        }

        return reinterpret_cast<Type *>(base + Handle);
    }
};
} // namespace JSlang
