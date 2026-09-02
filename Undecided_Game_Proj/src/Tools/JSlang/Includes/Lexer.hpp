#pragma once

#include "BitwiseCharacterClassifier.hpp"
#include "Diagnostics.hpp"
#include "ErrorCodes.hpp"
#include "StringHasher.hpp"
#include "SupportedEmbeddedLanguagesEnum.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>
#include <utility>

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
    KeyWord_Expect,
    KeyWord_From,
    KeyWord_Constant,
    KeyWord_Inline,
    KeyWord_Void,

    Identifier,
    EmbeddedLanguageCodeblock,

    FloatLiteral,   // 0.0
    IntegerLiteral, // 1
    StringLiteral,  // "Hello, world!"
    CharacterLiteral,

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

    Semicolon, // ;
    Dot,       // .

    Not,                  // !
    NotEqual,             // !=
    GreaterThanOrEqualTo, // >=
    LessThanOrEqualTo,    // <=

    AND, // &&
    OR,  // ||
    XOR, // ^

    EndOfFile,
    Invalid
};

struct Token
{
    TokenTypes     TokenType;
    SourceLocation ObjectSourceLocation;
};

// TODO: should do SIMD optimization with ISPC in the future. But it's still pretty fast, so
// probably no point in over engineering it
struct Lexer
{
    DiagnosticEngine &ObjectDiagnosticEngine;

    std::string_view Source;
    std::string_view Filename;
    size_t           Cursor = 0;
    uint32_t         Line   = 0;
    uint32_t         Column = 0;

    EmbeddedLanguageCodeblocks &ObjectEmbeddedLanguageCodeblocks;

    // oh god. this gives me enterprise java flashbacks
    Lexer(
        DiagnosticEngine           &ParameterDiagnosticEngine,
        EmbeddedLanguageCodeblocks &ParameterEmbeddedLanguageCodeblocks,
        std::string_view            ParameterSource,
        std::string_view            ParameterSourceFilename)
        : ObjectDiagnosticEngine(ParameterDiagnosticEngine), //
          Source(ParameterSource), Filename(ParameterSourceFilename),
          ObjectEmbeddedLanguageCodeblocks(ParameterEmbeddedLanguageCodeblocks)
    {
        if (Source.length() == 0)
        {
            ObjectDiagnosticEngine.Report(
                Severity::Warning,
                SOURCE_PROVIDED_IS_EMPTY,
                {.Filename = Filename},
                "The file provided is empty.",
                "Now... Ya thinking I'm a magician, mister? Expect me to whoop up an entire damn "
                "source file from your thoughts like a cheap chat bot? Take yer thoughts of making "
                "slop somewhere else! I ain't wastin' my dignity on this shit. Or excuse me, the "
                "lack thereof.");
        }
    };

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

    [[nodiscard]] bool match_next_character(char ExpectedCharacter) const
    {
        char character = peek_character_infront_cursor();
        return character == ExpectedCharacter;
    }

    Token make_token(TokenTypes TokenType, size_t TokenStart, size_t Length)
    {
        return {
            .TokenType            = TokenType,
            .ObjectSourceLocation = {
                .Source   = Source.substr(TokenStart, Length),
                .Filename = Filename,
                .Line     = Line,
                .Column   = Column}};
    }
    Token make_token(TokenTypes TokenType, std::string_view StringView)
    {
        return {
            .TokenType            = TokenType,
            .ObjectSourceLocation = {
                .Source = StringView, .Filename = Filename, .Line = Line, .Column = Column}};
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
        case "expect"_hash:
        {
            return TokenTypes::KeyWord_Expect;
        }
        case "from"_hash:
        {
            return TokenTypes::KeyWord_From;
        }
        case "const"_hash:
        {
            return TokenTypes::KeyWord_Constant;
        }
        case "inline"_hash:
        {
            return TokenTypes::KeyWord_Inline;
        }
        case "void"_hash:
        {
            return TokenTypes::KeyWord_Void;
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

    Token report_invalid_token(
        uint32_t    CursorStartPosition,
        Severity    Severity,
        uint32_t    ErrorCode,
        std::string ErrorMessage,
        std::string Monologue)
    {
        auto invalid_token = make_token(TokenTypes::Invalid, CursorStartPosition, 1);

        ObjectDiagnosticEngine.Report(
            Severity,
            ErrorCode,
            invalid_token.ObjectSourceLocation,
            std::move(ErrorMessage),
            std::move(Monologue));

        return invalid_token;
    }

    Token create_token_from_identifier_or_keyword(size_t CursorStartPosition)
    {
        using namespace BitwiseCharacterClassifier;

        while (IsIdentifierBody(peek_character_under_cursor()))
        {
            advance_one_character();
        };

        std::string_view string_view =
            Source.substr(CursorStartPosition, Cursor - CursorStartPosition);

        auto token_type = check_whether_identifier_or_keyword(string_view);

        if (token_type == TokenTypes::KeyWord_Begin)
        {
            auto language_identifier = GetNextToken(); // begin lua, where "lua" is the identifier
            uint32_t language_identifier_cursor_start_position = Cursor; // now it points after lua
            char     sentinel_buf[64];                                   // NOLINT
            auto sentinel_len = std::snprintf( // WARN: stack allocated and small. may cause some headaches to some poor guy later on
                sentinel_buf,
                sizeof(sentinel_buf),
                "end%.*s",
                static_cast<int>(language_identifier.ObjectSourceLocation.Source.size()),
                language_identifier.ObjectSourceLocation.Source.data());
            std::string_view expected_sentinel(
                sentinel_buf, sentinel_len); // endlua    begin blocks are rare enough where
                                             // dynamic allocation will be a drop in the bucket
                                             // compared to manually matching and figuring out
                                             // the proper casing for the sentinel using SIMD
                                             // optimization. it ain't just a problem we got

            size_t sentinel_position = Source.find(expected_sentinel, Cursor);
            if (sentinel_position == std::string::npos)
            {
                return report_invalid_token(
                    CursorStartPosition,
                    Severity::Fatal,
                    EMBEDDED_CODEBLOCK_NOT_TERMINATED,
                    "Embedded codeblock not terminated.",
                    std::format(
                        "Terminate yer god damn embedded codeblock with {}. You're the latest in a "
                        "string of idiots I've encountered.",
                        expected_sentinel));
            }

            auto language_index =
                GetEmbeddedLanguageEnumFromSource(language_identifier.ObjectSourceLocation.Source);

            if (!language_index.has_value())
            {
                return report_invalid_token(
                    language_identifier_cursor_start_position,
                    Severity::Fatal,
                    UNKNOWN_EMBEDDED_LANGUAGE,
                    language_index.error(),
                    std::format(
                        "Now, I've met some snake oil salesmen in my time. But, mister, you "
                        "take "
                        "the crown for the most delusional of them all. The hell is '{}'?!",
                        language_identifier.ObjectSourceLocation.Source));
            }

            if (language_index.value() > ObjectEmbeddedLanguageCodeblocks.size())
            {
                return report_invalid_token(
                    language_identifier_cursor_start_position,
                    Severity::Fatal,
                    UNKNOWN_EMBEDDED_LANGUAGE,
                    "Invalid embedded language.",
                    std::format(
                        "Now, some moron decided to add a language called '{}' that don't even "
                        "exist in terms "
                        "of implementation! Could ya believe it?",
                        language_identifier.ObjectSourceLocation.Source));
            }

            uint32_t advance_cursor_times = sentinel_position + expected_sentinel.size() - Cursor;
            for (uint32_t i = 0; i < advance_cursor_times; i++)
            {
                advance_one_character();
            }

            size_t substring_size  = sentinel_position - language_identifier_cursor_start_position;
            size_t underflow_guard = (substring_size > 0)
                                         ? substring_size - 1
                                         : 0; // -1 because sentinel_position will be pointing at
                                              // 'e' of "end"; we don't want that. we want the
                                              // whitespace before the 'e'
            ObjectEmbeddedLanguageCodeblocks[language_index.value()].push_back({
                .Source = Source.substr(language_identifier_cursor_start_position, underflow_guard),

                .Filename = language_identifier.ObjectSourceLocation.Filename,
                .Line     = language_identifier.ObjectSourceLocation.Line,
                .Column   = language_identifier.ObjectSourceLocation.Column,
            });

            // language_identifier.TokenType = TokenTypes::EmbeddedLanguageCodeblock;
            return GetNextToken();
        }

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

    Token
    create_token_from_string_literal(size_t CursorStartPosition, char ExpectedSentinelCharacter)
    {
        advance_one_character();
        while (peek_character_under_cursor() !=
                   ExpectedSentinelCharacter and // why not &&? why not!
               peek_character_under_cursor() != '\0')
        {
            advance_one_character();
        }

        if (peek_character_under_cursor() == '\0')
        {
            auto invalid_token =
                make_token(TokenTypes::Invalid, CursorStartPosition, Cursor - CursorStartPosition);

            ObjectDiagnosticEngine.Report(
                Severity::Fatal,
                UNTERMINATED_STRING,
                invalid_token.ObjectSourceLocation,
                "Unterminated string.",
                std::format(
                    "Ya forgot to put a {} after yer starting {}, mister. Thought you'd know after "
                    "all this damned time, but here we are...",
                    ExpectedSentinelCharacter,
                    ExpectedSentinelCharacter));

            return invalid_token;
        }

        if (ExpectedSentinelCharacter == '\'')
        {
            return make_token(
                TokenTypes::CharacterLiteral, CursorStartPosition, Cursor - CursorStartPosition);
        }
        return make_token(
            TokenTypes::StringLiteral, CursorStartPosition, Cursor - CursorStartPosition);
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

        size_t cursor_start_position = Cursor;

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
            return make_singular_token(TokenTypes::Semicolon, cursor_start_position);
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
        case '"':
        {
            return create_token_from_string_literal(cursor_start_position, '"');
        }
        case '\'':
        {
            return create_token_from_string_literal(cursor_start_position, '\'');
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

        return report_invalid_token(
            cursor_start_position,
            Severity::Error,
            UNKNOWN_SYMBOL,
            "Unknown symbol.",
            "Now get your ass and listen to me: what do you think I am, mister? A damned "
            "know-it-all? I ain't got the slightest clue what your petty little symbol here is "
            "meanin'.");
    }
};
} // namespace JSlang
