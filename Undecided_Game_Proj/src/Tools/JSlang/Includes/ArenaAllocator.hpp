
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
namespace JSlang
{
class ArenaAllocator
{
    size_t                 memory_chunk_size;
    size_t                 current_offset = 0;
    std::vector<uint8_t *> memory_chunks;

    void allocate_new_chunk()
    {
        auto *memory = static_cast<uint8_t *>(::operator new(memory_chunk_size));
        memory_chunks.push_back(memory);
        current_offset = 0;
    }

  public:
    explicit ArenaAllocator(size_t DefaultMemoryChunkSize = static_cast<size_t>(4 * 1024 * 1024))
        : memory_chunk_size(DefaultMemoryChunkSize)
    {
        allocate_new_chunk();
    }

    ~ArenaAllocator()
    {
        for (uint8_t *Chunk : memory_chunks)
        {
            ::operator delete(Chunk);
        }
    }

    ArenaAllocator(const ArenaAllocator &)            = delete;
    ArenaAllocator &operator=(const ArenaAllocator &) = delete;

    template <typename Type, typename... ArgumentTypes> Type *Allocate(ArgumentTypes &&...Arguments)
    {
        size_t alignment = alignof(Type);
        size_t size      = sizeof(Type);

        auto   current_pointer = reinterpret_cast<size_t>(memory_chunks.back() + current_offset);
        size_t aligned_pointer = (current_pointer + alignment - 1) & ~(alignment - 1);
        size_t padding         = aligned_pointer - current_pointer;

        if (current_offset + padding + size > memory_chunk_size)
        {
            allocate_new_chunk();
            return Allocate<Type>(std::forward<ArgumentTypes>(Arguments)...);
        }

        current_offset += padding + size;
        Type *result = reinterpret_cast<Type *>(aligned_pointer);

        return new (result) Type(std::forward<ArgumentTypes>(Arguments)...);
    }

    void Reset()
    {
        current_offset = 0;
        if (memory_chunks.size() > 1)
        {
            for (size_t i = 1; i < memory_chunks.size(); i++)
            {
                ::operator delete(memory_chunks[i]);
                memory_chunks.resize(1);
            }
        }
    }
};
} // namespace JSlang
