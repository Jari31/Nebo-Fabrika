#pragma once
#include "Lexer.hpp"
#include <cstdint>
#include <memory>
#include <utility>

namespace JSlang
{
struct ContentHasher
{
    std::unique_ptr<Lexer>    ObjectLexer;
    static constexpr uint64_t OFFSET_BASIS = 0xcbf29ce484222325;
    static constexpr uint64_t PRIME        = 0x100000001b3;

    ContentHasher(std::unique_ptr<Lexer> ParameterLexer)
    {
        ObjectLexer = std::move(ParameterLexer);
    };

    [[nodiscard]] uint64_t Hash64() const
    {
        uint64_t hash_value = OFFSET_BASIS;
        while (true)
        {
            hash_value ^= ObjectLexer->GetNextCharacter();
            hash_value = (hash_value * PRIME) & 0xFFFFFFFFFFFFFFFF;
        }
        return hash_value;
    }
};
}; // namespace JSlang
