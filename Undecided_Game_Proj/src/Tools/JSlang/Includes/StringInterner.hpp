#pragma once
#include "ArenaAllocator.hpp"
#include "DebugHelpers.hpp"
#include "Libraries/include/unordered_dense/ankerl/unordered_dense.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>

namespace JSlang
{
struct StringInterner
{
    ankerl::unordered_dense::map<std::string_view, uint32_t> InternHashmap;
    ArenaAllocator                                           StringInternArena;

    StringInterner()
    {
        if (!StringInternArena.Initialize())
        {
            DebugHelpers::BuiltinTrap();
        }
    };

    template <bool NullTerminate = true> uint32_t InternString(std::string_view String)
    {
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

        auto intern_handle = static_cast<uint32_t>(string_pointer - StringInternArena.base);
        InternHashmap.emplace(
            std::string_view(reinterpret_cast<char *>(string_pointer), length), intern_handle);

        return intern_handle;
    }

    template <bool SanityCheck = false>
    [[nodiscard]] std::string_view GetStringViewOverHandle(uint32_t Handle) const
    {
        uint8_t *length_pointer = StringInternArena.base + Handle;

        if constexpr (SanityCheck)
        {
            if (length_pointer + sizeof(uint32_t) >
                StringInternArena.base + StringInternArena.committed_size)
            {
                std::cerr << "HANDLE IS OUT OF BOUNDS; WHILST TRYING TO PARSE STRING VIEW OUT OF "
                             "HANDLE.\n IN FUNCTION: "
                          << __FUNCTION__ << "\n";
                DebugHelpers::BuiltinTrap();
            }
        }

        uint32_t length = 0;
        std::memcpy(&length, length_pointer, sizeof(uint32_t));

        uint8_t *string_pointer = length_pointer + sizeof(uint32_t);

        if constexpr (SanityCheck)
        {
            if (string_pointer + length > StringInternArena.base + StringInternArena.committed_size)
            {
                std::cerr << "STRING VIEW IS OUT OF BOUNDS.\n IN FUNCTION: " << __FUNCTION__
                          << "\n";
                DebugHelpers::BuiltinTrap();
            }
        }

        return {reinterpret_cast<char *>(string_pointer), length};
    }
};
} // namespace JSlang
