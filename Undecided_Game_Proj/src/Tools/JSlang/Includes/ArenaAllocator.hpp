
#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <memoryapi.h>
#include <winnt.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "DebugHelpers.hpp"
#include "iostream"
#include <cstdint>
#include <cstring>
#include <span>

namespace JSlang
{

struct ArenaAllocator
{
    using Handle = uint32_t;

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
        std::cerr << "\nARENA ALLOCATOR OUT OF MEMORY.\n"
                  << "FAILED TO ALLOCATE " << RequestedBytes << " BYTES.\n"
                  << "Reserved 4GB virtual address space exhausted.\n";

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

    void commit_pages_given_target_size(size_t TargetSize)
    {
        if (TargetSize > reserved_capacity) [[unlikely]]
        {
            PANIC_OUT_OF_MEMORY(TargetSize);
        }

        if ((TargetSize) > committed_size) [[unlikely]]
        {
            size_t target_commit = (TargetSize + 0xFFFFU) & ~0xFFFFU;

            commit_pages_given_target_size(target_commit);

            if (!CommitPages(target_commit)) [[unlikely]]
            {
                PANIC_OUT_OF_MEMORY(TargetSize);
            };
        }
    };

    uint8_t *AllocateBlockOfSize(size_t Size, size_t Alignment = 1)
    {
        auto *current_pointer = base + current_offset;

        auto   pointer_value   = reinterpret_cast<std::uintptr_t>(current_pointer);
        size_t aligned_pointer = (pointer_value + Alignment - 1) & ~(Alignment - 1);
        size_t padding         = aligned_pointer - pointer_value;

        size_t new_offset = current_offset + padding + Size;

        commit_pages_given_target_size(new_offset);

        current_offset += new_offset;
        return current_pointer + padding;
    }
    template <typename Type, typename... ArgumentTypes> Type *Allocate(ArgumentTypes &&...Arguments)
    {
        size_t alignment = alignof(Type);
        size_t size      = sizeof(Type);

        auto   current_pointer = reinterpret_cast<size_t>(base + current_offset);
        size_t aligned_pointer = (current_pointer + alignment - 1) & ~(alignment - 1);
        size_t padding         = aligned_pointer - current_pointer;

        size_t new_offset = current_pointer + padding + size;

        commit_pages_given_target_size(new_offset);

        current_offset += new_offset;
        Type *result = reinterpret_cast<Type *>(aligned_pointer);

        new (result) Type(std::forward<ArgumentTypes>(Arguments)...);

        return result;
    }
    template <typename Type> Handle ConvertPtrToHandle(Type *Pointer)
    {
        return reinterpret_cast<Handle>(Pointer - base);
    };
    /// assumes handle is already aligned.
    template <typename Type> Type *ConvertHandleToPtr(uint32_t Handle)
    {
        assert(
            static_cast<uint64_t>(Handle) + sizeof(Type) <= committed_size &&
            "Handle out of bounds.");

        return reinterpret_cast<Type *>(base + Handle);
    }
    template <typename Type> Handle AllocateArray(size_t Count)
    {
        // [ uint32_t header ] ... objects ...

        if (Count == 0) [[unlikely]]
        {
            return {};
        }

        size_t alignment = alignof(Type);
        size_t size      = sizeof(Type) * Count;

        auto *header_pointer = AllocateBlockOfSize(sizeof(uint32_t), alignof(uint32_t));

        memcpy(header_pointer, &Count, sizeof(uint32_t));

        size_t current_pointer = reinterpret_cast<size_t>(header_pointer + sizeof(uint32_t));
        size_t aligned_pointer = (current_pointer + alignment - 1) & ~(alignment - 1);
        size_t padding         = aligned_pointer - current_pointer;

        size_t new_offset = padding + size + current_offset;

        commit_pages_given_target_size(new_offset);

        current_offset += padding + size;
        auto *resulting_array = reinterpret_cast<Type *>(aligned_pointer);

        for (size_t i = 0; i < Count; i++)
        {
            new (&resulting_array[i]) Type();
        }

        return static_cast<Handle>(header_pointer - base);
    }

    template <typename Type, bool SanityCheck = false>
    std::span<Type> GetSpanFromHandle(Handle ParameterHandle)
    {
        uint8_t *count_pointer = base + ParameterHandle;

        if constexpr (SanityCheck)
        {
            if (ParameterHandle > committed_size)
            {
                std::cerr << "ATTEMPT TO READ SPAN COUNT OUT OF BOUNDS OF ARENA.\n";

                DebugHelpers::BuiltinTrap();
            }
        }

        uint32_t count = 0;

        memcpy(&count, count_pointer, sizeof(uint32_t));

        size_t alignment = alignof(Type);

        size_t content_offset_unaligned = ParameterHandle + sizeof(uint32_t);
        size_t aligned_offset = (content_offset_unaligned + alignment - 1) & ~(alignment - 1);

        if constexpr (SanityCheck)
        {
            if (content_offset_unaligned + (count * sizeof(Type)) > committed_size)
            {
                std::cerr << "ATTEMPT TO READ SPAN CONTENT OUT OF BOUNDS OF ARENA.\n";

                DebugHelpers::BuiltinTrap();
            }
        }

        auto *content_pointer = reinterpret_cast<Type *>(base + aligned_offset);
        return std::span<Type>(content_pointer, count);
    }
};
} // namespace JSlang
