#pragma once

#include "Includes/BitwiseCharacterClassifier.hpp"
#include "Includes/StringHasher.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace JSlang
{
enum class TokenTypes : uint8_t
{
    KeyWord_Uniform,
    KeyWord_Export,
    KeyWord_Using,
    KeyWord_Discard,
    KeyWord_Alias,

    Identifier,
    FloatLiteral,
    IntegerLiteral,
    StringLiteral,

    AtSymbol,
    Equal,
    EqualEqual,
    Plus,
    PlusPlus,
    Minus,
    MinusMinus,
    Star,
    Slash,
    LeftParenthesis,
    RightParenthesis,
    LeftBracket,
    RightBracket,

    EndOfFile,
    Invalid
};

struct Token
{
    TokenTypes       TokenType;
    std::string_view Source;
    uint32_t         Line;
    uint32_t         Column;
};

struct Lexer
{
    std::string_view Source;
    size_t           Cursor = 0;
    uint32_t         Line   = 0;
    uint32_t         Column = 0;

    [[nodiscard]] char peek_character_under_cursor() const
    {
        return (Cursor < Source.length()) ? Source[Cursor] : '\0';
    }

    char peek_and_advance_one_character()
    {
        char character = peek_character_under_cursor();
        ++Cursor;
        if (character == '\n')
        {
            ++Line;
            Column = 1;
        }
        else
        {
            ++Column;
        }
        return character;
    }

    bool match_next_character(char ExpectedCharacter)
    {
        char character = peek_and_advance_one_character();
        return character == ExpectedCharacter;
    }

    Token make_token(TokenTypes TokenType, size_t TokenStart, size_t Length)
    {
        return {
            .TokenType = TokenType,
            .Source    = Source.substr(TokenStart, Length),
            .Line      = Line,
            .Column    = Column};
    }
    Token make_token(TokenTypes TokenType, std::string_view StringView)
    {
        return {.TokenType = TokenType, .Source = StringView, .Line = Line, .Column = Column};
    }

    static TokenTypes check_whether_identifier_or_keyword(std::string_view Text)
    {
        using namespace StringHasher;
        if (Text.length() > 12) // WARN change this if adding bigger keywords in the future
        {
            return TokenTypes::Identifier;
        }

        switch (HashString(Text))
        {
        case "uniform"_hash:
        {
            return TokenTypes::KeyWord_Uniform;
        }
        case "export"_hash:
        {
            return TokenTypes::KeyWord_Export;
        }
        case "using"_hash:
        {
            return TokenTypes::KeyWord_Using;
        }
        case "discard"_hash:
        {
            return TokenTypes::KeyWord_Discard;
        }
        default:
        {
            return TokenTypes::Identifier;
        }
        }
    }

    Token create_token_from_identifier_or_keyword(char Character, size_t CursorStartPosition)
    {
        while (BitwiseCharacterClassifier::IsIdentifierBody(peek_and_advance_one_character()))
        {
        };

        std::string_view string_view =
            Source.substr(CursorStartPosition, Cursor - CursorStartPosition);
        auto token_type = check_whether_identifier_or_keyword(string_view);

        return make_token(token_type, string_view);
    }

    Token GetNextToken()
    {
        size_t cursor_start_position = Cursor;
        char   character             = peek_character_under_cursor();
        switch (character)
        {
        case '\0':
        {
            return make_token(TokenTypes::EndOfFile, cursor_start_position, 1);
        }
        case '@':
        {
            return make_token(TokenTypes::AtSymbol, cursor_start_position, 1);
        }
        case '=': // could use a macro, but would be annoyingly complex to maintain
        {
            if (match_next_character('='))
            {
                return make_token(TokenTypes::EqualEqual, cursor_start_position, 2);
            }
            return make_token(TokenTypes::Equal, cursor_start_position, 1);
        }
        case '-':
        {
            if (match_next_character('-'))
            {
                return make_token(TokenTypes::MinusMinus, cursor_start_position, 1);
            }
            return make_token(TokenTypes::Minus, cursor_start_position, 1);
        }
        case '+':
        {
            if (match_next_character('+'))
            {
                return make_token(TokenTypes::PlusPlus, cursor_start_position, 1);
            }
            return make_token(TokenTypes::Plus, cursor_start_position, 1);
        }
        default:
            break;
        }

        if (BitwiseCharacterClassifier::IsIdentifierStart(character))
        {
            return create_token_from_identifier_or_keyword(character, cursor_start_position);
        }

        return make_token(TokenTypes::Invalid, cursor_start_position, 1);
    }
};
} // namespace JSlang
