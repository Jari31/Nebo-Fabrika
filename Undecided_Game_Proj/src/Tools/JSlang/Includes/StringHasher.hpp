#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace JSlang::StringHasher
{
constexpr uint64_t HashString(std::string_view String)
{
    uint64_t hash = 14695981039346656037ULL;
    for (char Character : String)
    {
        hash ^= static_cast<uint64_t>(Character);
        hash *= 1099511628211ULL;
    }

    return hash;
}

constexpr uint64_t operator""_hash(const char *String, size_t Length)
{
    return HashString(std::string_view(String, Length));
}
} // namespace JSlang::StringHasher
