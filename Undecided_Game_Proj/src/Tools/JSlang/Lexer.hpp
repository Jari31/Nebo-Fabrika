#pragma once

#include "Includes/BitwiseCharacterClassifier.hpp"
#include "Includes/StringHasher.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
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
    KeyWord_Begin,
    KeyWord_End,

    Identifier,

    FloatLiteral,   // 0.0
    IntegerLiteral, // 1
    StringLiteral,  // "Hello, world!"

    AtSymbol,   // @
    Equal,      // =
    EqualEqual, // ==
    Plus,       // +
    PlusPlus,   // ++
    PlusEqual,  // +=
    Minus,      // -
    MinusMinus, // --
    MinusEqual, // -=
    Star,       // *
    StarEqual,  // *=
    Slash,      // /
    SlashEqual, // /=
    BackSlash,
    Ampersand, // &
    Pipe,      // |

    LeftParenthesis,    // (
    RightParenthesis,   // )
    LeftBracket,        // {
    RightBracket,       // }
    Comma,              // ,
    LeftSquareBracket,  // [
    RightSquareBracket, // ]
    LeftAngleBracket,   // <
    RightAngleBracket,  // >

    SemiColon, // ;
    Dot,       // .

    Not,                  // !
    NotEqual,             // !=
    GreaterThanOrEqualTo, // >=
    LessThanOrEqualTo,    // <=

    AND, // &&
    OR,  // ||
    XOR, // ^

    InlineBegin,
    InlineEnd,

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

// TODO: should do SIMD optimization with ISPC in the future. But it's still pretty fast, so
// probably no point in over engineering it
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

    [[nodiscard]] char peek_character_infront_cursor() const
    {
        return (Cursor + 1 < Source.length()) ? Source[Cursor + 1] : '\0';
    }

    char advance_one_character()
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

    bool match_next_character(char ExpectedCharacter) const
    {
        char character = peek_character_infront_cursor();
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
    Token make_token(TokenTypes TokenType, size_t CursorStartPosition)
    {
        return make_token(TokenType, CursorStartPosition, 1);
    }

    /// increments the cursor by one
    Token make_singular_token(TokenTypes TokenType, size_t CursorStartPosition)
    {
        advance_one_character();
        return make_token(TokenType, CursorStartPosition, 1);
    }

    /// increments the cursor by two
    Token make_dual_token(TokenTypes TokenType, size_t CursorStartPosition)
    {
        advance_one_character();
        advance_one_character();

        return make_token(TokenType, CursorStartPosition, 2);
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
        case "begin"_hash:
        {
            return TokenTypes::KeyWord_Begin;
        }
        case "end"_hash:
        {
            return TokenTypes::KeyWord_End;
        }
        default:
        {
            return TokenTypes::Identifier;
        }
        }
    }

    Token skip_comment()
    {
        while (peek_character_under_cursor() != '\n')
        {
            advance_one_character();
        }

        return GetNextToken();
    }

    Token create_token_from_identifier_or_keyword(size_t CursorStartPosition)
    {
        while (BitwiseCharacterClassifier::IsIdentifierBody(peek_character_under_cursor()))
        {
            advance_one_character();
        };

        std::string_view string_view =
            Source.substr(CursorStartPosition, Cursor - CursorStartPosition);

        auto token_type = check_whether_identifier_or_keyword(string_view);

        return make_token(token_type, string_view);
    }

    Token create_token_from_digits(size_t CursorStartPosition)
    {
        while (BitwiseCharacterClassifier::IsDigit(peek_character_under_cursor()))
        {
            advance_one_character();
        }

        if (advance_one_character() == '.')
        {
            if (!BitwiseCharacterClassifier::IsDigit(peek_character_under_cursor()))
            {
                return make_token(
                    TokenTypes::Invalid, CursorStartPosition, Cursor - CursorStartPosition);
            }

            while (BitwiseCharacterClassifier::IsDigit(peek_character_under_cursor()))
            {

                advance_one_character();
            }

            if (peek_character_under_cursor() == 'f' || peek_character_under_cursor() == 'F')
            {
                advance_one_character();
            }

            return make_token(
                TokenTypes::FloatLiteral, CursorStartPosition, Cursor - CursorStartPosition);
        }

        return make_token(
            TokenTypes::IntegerLiteral, CursorStartPosition, Cursor - CursorStartPosition);
    }

    void skip_whitespaces()
    {
        while (BitwiseCharacterClassifier::IsWhitespace(peek_character_under_cursor()))
        {
            advance_one_character();
        }
    }

    Token GetNextToken()
    {
        skip_whitespaces();

        char character = peek_character_under_cursor();

        size_t cursor_start_position = Cursor; // what

        switch (character)
        {
        case '\0':
        {
            return make_singular_token(TokenTypes::EndOfFile, cursor_start_position);
            ;
        }
        case '@':
        {
            return make_singular_token(TokenTypes::AtSymbol, cursor_start_position);
        }
        case '=': // could use a macro, but would be annoyingly complex to maintain
        {
            if (match_next_character('='))
            {
                return make_dual_token(TokenTypes::EqualEqual, cursor_start_position);
            }
            return make_singular_token(TokenTypes::Equal, cursor_start_position);
        }
        case '-':
        {
            switch (peek_character_infront_cursor())
            {
            case '-':
            {
                return make_dual_token(TokenTypes::MinusMinus, cursor_start_position);
            }
            case '=':
            {
                return make_dual_token(TokenTypes::MinusEqual, cursor_start_position);
            }
            default:
            {
                break;
            }
            }
            return make_singular_token(TokenTypes::Minus, cursor_start_position);
        }
        case '+':
        {
            if (match_next_character('+'))
            {
                return make_dual_token(TokenTypes::PlusPlus, cursor_start_position);
            }
            return make_singular_token(TokenTypes::Plus, cursor_start_position);
        }
        case '/':
        {
            switch (peek_character_infront_cursor())
            {
            case '/':
            {
                return skip_comment();
            }
            case '=':
            {
                return make_dual_token(TokenTypes::SlashEqual, cursor_start_position);
            }
            default:
            {
                break;
            }
            }
            return make_singular_token(TokenTypes::Slash, cursor_start_position);
        }
        case '(':
        {
            return make_singular_token(TokenTypes::LeftParenthesis, cursor_start_position);
        }
        case ')':
        {
            return make_singular_token(TokenTypes::RightParenthesis, cursor_start_position);
        }
        case '{':
        {
            return make_singular_token(TokenTypes::LeftBracket, cursor_start_position);
        }
        case '}':
        {
            return make_singular_token(TokenTypes::RightBracket, cursor_start_position);
        }
        case ',':
        {
            return make_singular_token(TokenTypes::Comma, cursor_start_position);
        }
        case '[':
        {
            return make_singular_token(TokenTypes::LeftSquareBracket, cursor_start_position);
        }
        case ']':
        {
            return make_singular_token(TokenTypes::RightSquareBracket, cursor_start_position);
        }
        case ';':
        {
            return make_singular_token(TokenTypes::SemiColon, cursor_start_position);
        }
        case '*':
        {
            if (match_next_character('='))
            {
                return make_dual_token(TokenTypes::StarEqual, cursor_start_position);
            }
            return make_singular_token(TokenTypes::Star, cursor_start_position);
        }
        case '.':
        {
            return make_singular_token(TokenTypes::Dot, cursor_start_position);
        }
        case '!':
        {
            if (match_next_character('='))
            {
                return make_dual_token(TokenTypes::NotEqual, cursor_start_position);
            }

            return make_singular_token(TokenTypes::Not, cursor_start_position);
        }
        case '>':
        {
            if (match_next_character('='))
            {
                return make_dual_token(TokenTypes::GreaterThanOrEqualTo, cursor_start_position);
            }
            return make_singular_token(TokenTypes::RightAngleBracket, cursor_start_position);
        }
        case '<':
        {
            if (match_next_character('='))
            {
                return make_dual_token(TokenTypes::LessThanOrEqualTo, cursor_start_position);
            }

            return make_singular_token(TokenTypes::LeftAngleBracket, cursor_start_position);
        }
        case '&':
        {
            if (match_next_character('&'))
            {
                return make_dual_token(TokenTypes::AND, cursor_start_position);
            }

            return make_singular_token(TokenTypes::Ampersand, cursor_start_position);
        }
        case '|':
        {
            if (match_next_character('|'))
            {
                return make_dual_token(TokenTypes::OR, cursor_start_position);
            }

            return make_singular_token(TokenTypes::Pipe, cursor_start_position);
        }
        case '^':
        {
            return make_singular_token(TokenTypes::XOR, cursor_start_position);
        }
        default:
            break;
        }

        if (BitwiseCharacterClassifier::IsIdentifierStart(character))
        {
            return create_token_from_identifier_or_keyword(cursor_start_position);
        }
        if (BitwiseCharacterClassifier::IsDigit(character))
        {
            return create_token_from_digits(cursor_start_position);
        }

        // if (BitwiseCharacterClassifier::IsWhitespace(character))
        // {
        //     return make_token(TokenTypes::EndOfFile, cursor_start_position, 1);
        // }

        return make_token(TokenTypes::Invalid, cursor_start_position, 1);
    }
};
} // namespace JSlang
