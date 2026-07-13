#pragma once
#include <cstdlib>
#include <cstdint>

struct Arena {
    uint8_t* Buffer;
    size_t Size;
    size_t Offset;
};

inline void ArenaInit(Arena* Self, size_t Size)
{
    Self->Buffer = (uint8_t*)malloc(Size);
    Self->Size = Size;
    Self->Offset = 0;
}

inline void* ArenaAllocate(Arena* Self, size_t Size, )
