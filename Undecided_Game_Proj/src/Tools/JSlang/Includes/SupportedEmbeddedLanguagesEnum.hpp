#pragma once

#include "Diagnostics.hpp"
#include "StringHasher.hpp"
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace JSlang
{
using EmbeddedLanguageCodeblocks = std::vector<std::vector<SourceLocation>>;

enum class SupportedEmbeddedLanguages : uint8_t
{
    LuaJIT,
};

std::expected<uint8_t, std::string> GetEmbeddedLanguageEnumFromSource(std::string_view Source)
{
    using namespace StringHasher;
    switch (HashString(Source))
    {
    case "lua"_hash:
    case "Lua"_hash:
    {
        return std::to_underlying(SupportedEmbeddedLanguages::LuaJIT);
    }
    default:
        return std::unexpected("Unknown embedded language.");
    }
}
} // namespace JSlang
