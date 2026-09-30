#pragma once
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

struct VirtualArena
{
    uint8_t *base              = nullptr;
    size_t   reserved_capacity = 0;
    size_t   committed_size    = 0;

    bool Init(size_t ReserveBytes = 4ULL * 1024 * 1024 * 1024)
    {
        reserved_capacity = ReserveBytes;

#if defined(_WIN32)
        base = static_cast<uint8_t *>(
            VirtualAlloc(NULL, reserved_capacity, MEM_RESERVE, PAGE_NOACCESS));
#else
        base = static_cast<uint8_t *>(
            mmap(NULL, reserved_capacity, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0));
        if (base == MAP_FAILED) base = nullptr;
#endif

        return base != nullptr;
    }

    bool CommitPages(size_t BytesToCommit)
    {
        if (BytesToCommit <= committed_size) return true;
        if (BytesToCommit > reserved_capacity) return false;

        size_t size_needed = BytesToCommit - committed_size;

#if defined(_WIN32)
        void *result = VirtualAlloc(base + committed_size, size_needed, MEM_COMMIT, PAGE_READWRITE);
        if (!result) return false;
#else
        if (mprotect(base + committed_size, size_needed, PROT_READ | PROT_WRITE) != 0)
        {
            return false;
        }
#endif

        committed_size = BytesToCommit;
        return true;
    }

    void Release()
    {
        if (!base) return;
#if defined(_WIN32)
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, reserved_capacity);
#endif
        base           = nullptr;
        committed_size = 0;
    }
};
