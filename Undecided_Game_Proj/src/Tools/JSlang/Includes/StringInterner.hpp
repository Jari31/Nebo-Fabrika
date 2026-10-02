#pragma once
#include "ArenaAllocator.hpp"
#include "DebugHelpers.hpp"
#include "Libraries/include/unordered_dense/ankerl/unordered_dense.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>

namespace JSlang
{
struct StringInterner
{
    using Handle = ArenaAllocator::Handle;

    ankerl::unordered_dense::map<std::string_view, uint32_t> InternHashmap;
    ArenaAllocator                                           StringInternArena;

    StringInterner()
    {
        if (!StringInternArena.Initialize())
        {
            DebugHelpers::BuiltinTrap();
        }

        StringInternArena.AllocateBlockOfSize(
            1); // so the handle is never 0, letting it act as the unknown value
    };

    template <bool NullTerminate = true> Handle InternString(std::string_view String)
    {
        if (String == "")
        {
            return 0;
        }

        auto iterator = InternHashmap.find(String);
        if (iterator != InternHashmap.end())
        {
            return iterator->second;
        }

        auto     length = static_cast<uint32_t>(String.size());
        uint8_t *memory_block;

        if constexpr (NullTerminate)
        {
            memory_block = StringInternArena.AllocateBlockOfSize(
                length + sizeof(uint32_t) + 1, alignof(uint32_t));
        }
        else
        {
            memory_block =
                StringInternArena.AllocateBlockOfSize(length + sizeof(uint32_t), alignof(uint32_t));
        }

        std::memcpy(memory_block, &length, sizeof(uint32_t));

        uint8_t *string_pointer = memory_block + sizeof(uint32_t);
        std::memcpy(string_pointer, String.data(), length);
        if constexpr (NullTerminate)
        {
            string_pointer[length] = '\0';
        }

        auto intern_handle = static_cast<Handle>(string_pointer - StringInternArena.base);
        InternHashmap.emplace(
            std::string_view(reinterpret_cast<char *>(string_pointer), length), intern_handle);

        return intern_handle;
    }

    template <bool SanityCheck = false>
    [[nodiscard]] std::string_view GetStringViewOverHandle(Handle ParameterHandle) const
    {
        if (ParameterHandle == 0)
        {
            return "";
        }

        uint8_t *length_pointer = StringInternArena.base + ParameterHandle;

        assert(
            length_pointer + sizeof(uint32_t) <=
                StringInternArena.base + StringInternArena.committed_size &&
            "HANDLE IS OUT OF BOUNDS; WHILST TRYING TO PARSE STRING VIEW OUT OF HANDLE.");

        uint32_t length = 0;
        std::memcpy(&length, length_pointer, sizeof(uint32_t));

        uint8_t *string_pointer = length_pointer + sizeof(uint32_t);

        assert(
            string_pointer + length <= StringInternArena.base + StringInternArena.committed_size &&
            "STRING VIEW IS OUT OF BOUNDS.");

        return {reinterpret_cast<char *>(string_pointer), length};
    }
};
} // namespace JSlang
