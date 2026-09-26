#pragma once

#include "Libraries/include/unordered_dense/ankerl/unordered_dense.h"
#include "Libraries/include/xxhash/xxhash.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace JSlang::SemanticAnalysis
{
enum class SymbolTypes : uint8_t
{
    Variable,
    Function,
    Struct,
    Alias,
    Decorator,
    Type
};

struct Symbol
{
    std::string_view Name;
    SymbolTypes      SymbolType;
    Symbol          *AliasOf;
};

enum class ScopeTypes : uint8_t
{
    Function,
    Block
};

struct PreHashedStringView
{
    std::string_view StringView;
    uint64_t         Hash;

    bool operator==(const PreHashedStringView &ComparedTo) const noexcept
    {
        return Hash == ComparedTo.Hash && StringView == ComparedTo.StringView;
    }
};

struct TransparentPreHash
{
    using is_transparent = void;

    std::size_t operator()(const PreHashedStringView &Key) const noexcept
    {
        return static_cast<std::size_t>(Key.Hash);
    }

    std::size_t operator()(uint64_t HashValue) const noexcept
    {
        return static_cast<std::size_t>(HashValue);
    }
};

using SymbolMap = ankerl::unordered_dense::map<PreHashedStringView, Symbol *, TransparentPreHash>;

struct GlobalScope
{
    SymbolMap ObjectSymbolMap;

    [[nodiscard]] Symbol *FindSymbolInCurrentScope(PreHashedStringView WithName)
    {
        if (ObjectSymbolMap.contains(WithName))
        {
            return ObjectSymbolMap.at(WithName);
        }

        return nullptr;
    }
};

struct Scope
{
    Scope     *ParentScope;
    ScopeTypes ScopeType;

    std::span<Symbol *> Symbols;

    [[nodiscard]] Symbol *FindSymbolInCurrentScope(std::string_view WithName) const
    {
        for (auto *Symbol : Symbols)
        {
            if (Symbol->Name == WithName)
            {
                return Symbol;
            }
        }

        return nullptr;
    }
};

struct SemanticAnalyzer
{
};
}; // namespace JSlang::SemanticAnalysis
