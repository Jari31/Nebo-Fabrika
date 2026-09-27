#pragma once

#include "BitwiseCharacterClassifier.hpp"
#include "Diagnostics.hpp"
#include "ErrorCodes.hpp"
#include "Libraries/include/xxhash/xxhash.h"
#include "StringHasher.hpp"
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
    UNKNOWN,

    Keyword_Uniform,
    Keyword_Import,
    Keyword_As,
    Keyword_Unsafe,
    Keyword_Using,
    Keyword_Discard,
    Keyword_Alias,
    Keyword_Expect,
    Keyword_From,
    Keyword_Constant,
    Keyword_If,
    Keyword_Else,
    Keyword_MutableVariable,     // var
    Keyword_FunctionDeclaration, // fn
    Keyword_Return,
    Keyword_Switch,
    Keyword_Case,
    Keyword_Struct,
    Keyword_Default,
    Keyword_For,
    Keyword_While,
    Keyword_Break,
    Keyword_Continue,
    Keyword_CompileTime,
    Keyword_DefineMacro,

    Identifier,

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
    LeftBrace,          // {
    RightBrace,         // }
    Comma,              // ,
    LeftSquareBracket,  // [
    RightSquareBracket, // ]
    LeftAngleBracket,   // <
    RightAngleBrace,    // >

    RightArrow, // ->

    Semicolon,  // ;
    Colon,      // :
    ColonColon, // ::
    Dot,        // .
    Ellipsis,   // ..

    Not,                  // !
    NotEqual,             // !=
    GreaterThanOrEqualTo, // >=
    LessThanOrEqualTo,    // <=

    AND, // &&
    OR,  // ||
    XOR, // ^

    EmbeddedCodeblock,

    EndOfFile,
    Invalid
};

struct Token
{
    TokenTypes     TokenType = TokenTypes::UNKNOWN;
    SourceLocation ObjectSourceLocation;
};

// TODO: should do SIMD optimization with ISPC in the future. But it's still pretty fast, so
// probably no point in over engineering it
struct Lexer
{
    static constexpr char UNDEFINED_NAME[10] = "Undefined"; // NOLINT

    DiagnosticEngine &ObjectDiagnosticEngine;

    std::string_view Source;
    std::string_view Filename;
    size_t           Cursor = 0;
    uint32_t         Line   = 0;
    uint32_t         Column = 0;

    // oh god. this gives me enterprise java flashbacks
    Lexer(
        DiagnosticEngine &ParameterDiagnosticEngine,
        std::string_view  ParameterSource,
        std::string_view  ParameterFilename)
        : ObjectDiagnosticEngine(ParameterDiagnosticEngine)
    {
        if (ParameterFilename.empty())
        {
            Filename = std::string_view{UNDEFINED_NAME};

            ObjectDiagnosticEngine.Report(
                Severity::Warning,
                SOURCE_PROVIDED_IS_EMPTY,
                {.Filename = Filename},
                "No filename provided; assuming 'Undefined' as filename.",
                "Suit yourself.");
        }
        else
        {
            Filename = ParameterFilename;
        }

        if (ParameterSource.empty())
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
        else
        {
            Source = ParameterSource;
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

    void start_new_line()
    {
        ++Line;
        Column = 1;
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
    Token make_token(TokenTypes TokenType, std::string_view StringView, uint64_t StringViewHash)
    {
        return {
            .TokenType            = TokenType,
            .ObjectSourceLocation = {
                .Source     = StringView,
                .SourceHash = StringViewHash,
                .Filename   = Filename,
                .Line       = Line,
                .Column     = Column}};
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
            return TokenTypes::Keyword_Uniform;
        }
        case "using"_hash:
        {
            return TokenTypes::Keyword_Using;
        }
        case "discard"_hash:
        {
            return TokenTypes::Keyword_Discard;
        }
        case "expect"_hash:
        {
            return TokenTypes::Keyword_Expect;
        }
        case "from"_hash:
        {
            return TokenTypes::Keyword_From;
        }

        case "const"_hash:
        {
            return TokenTypes::Keyword_Constant;
        }

        case "if"_hash:
        {
            return TokenTypes::Keyword_If;
        }
        case "else"_hash:
        {
            return TokenTypes::Keyword_Else;
        }
        case "var"_hash:
        {
            return TokenTypes::Keyword_MutableVariable;
        }
        case "fn"_hash:
        {
            return TokenTypes::Keyword_FunctionDeclaration;
        }
        case "return"_hash:
        {
            return TokenTypes::Keyword_Return;
        }
        case "struct"_hash:
        {
            return TokenTypes::Keyword_Struct;
        }
        case "switch"_hash:
        {
            return TokenTypes::Keyword_Switch;
        }
        case "case"_hash:
        {
            return TokenTypes::Keyword_Case;
        }
        case "default"_hash:
        {
            return TokenTypes::Keyword_Default;
        }
        case "for"_hash:
        {
            return TokenTypes::Keyword_For;
        }
        case "while"_hash:
        {
            return TokenTypes::Keyword_While;
        }
        case "break"_hash:
        {
            return TokenTypes::Keyword_Break;
        }
        case "continue"_hash:
        {
            return TokenTypes::Keyword_Continue;
        }
        case "import"_hash:
        {
            return TokenTypes::Keyword_Import;
        }
        case "unsafe"_hash:
        {
            return TokenTypes::Keyword_Unsafe;
        }
        case "as"_hash:
        {
            return TokenTypes::Keyword_As;
        }
        case "alias"_hash:
        {
            return TokenTypes::Keyword_Alias;
        }
        case "comptime"_hash:
        {
            return TokenTypes::Keyword_CompileTime;
        }
        case "define_macro"_hash:
        {
            return TokenTypes::Keyword_DefineMacro;
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
        ErrorCodes  ErrorCode,
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

        if (token_type != TokenTypes::Identifier)
        {
            return make_token(token_type, string_view);
        }

        return make_token(
            token_type, string_view, XXH64(string_view.data(), string_view.size(), 0));
    }

    Token create_token_from_digits(size_t CursorStartPosition)
    {
        while (BitwiseCharacterClassifier::IsDigit(peek_character_under_cursor()))
        {
            advance_one_character();
        }

        if (peek_character_under_cursor() == '.')
        {
            advance_one_character();
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

    template <char ExpectedSentinelCharacter>
    Token create_token_from_string_literal(size_t CursorStartPosition)
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
                Severity::Error,
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

        advance_one_character(); // consume the trailing string terminator

        if constexpr (ExpectedSentinelCharacter == '\'')
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
            return make_token(TokenTypes::EndOfFile, cursor_start_position, 1);
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
            case '>':
            {
                return make_dual_token(TokenTypes::RightArrow, cursor_start_position);
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
            return make_singular_token(TokenTypes::LeftBrace, cursor_start_position);
        }
        case '}':
        {
            return make_singular_token(TokenTypes::RightBrace, cursor_start_position);
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
            if (match_next_character('.'))
            {
                return make_dual_token(TokenTypes::Ellipsis, cursor_start_position);
            }

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
            return make_singular_token(TokenTypes::RightAngleBrace, cursor_start_position);
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
            return create_token_from_string_literal<'"'>(cursor_start_position);
        }
        case '\'':
        {
            return create_token_from_string_literal<'\''>(cursor_start_position);
        }
        case ':':
        {
            if (match_next_character(':'))
            {
                return make_dual_token(TokenTypes::ColonColon, cursor_start_position);
            }

            return make_singular_token(TokenTypes::Colon, cursor_start_position);
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

    char8_t GetNextCharacter()
    {
        skip_whitespaces();

        if (peek_character_under_cursor() == '/' && peek_character_infront_cursor() == '/')
        {
            skip_comment();
        }

        return advance_one_character();
    }
};
} // namespace JSlang
