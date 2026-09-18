#pragma once
#include "ASTNodes.hpp"
#include "ArenaAllocator.hpp"
#include "Diagnostics.hpp"
#include "ErrorCodes.hpp"
#include "Lexer.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <print>
#include <span>
#include <stacktrace>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace JSlang::AST
{
struct Parser
{
    /*
     * Attributes could be of three types:
     *
     *  Variable attributes:
     *      var Identifier: Attributes = Initializer;
     *      OR
     *      var Identifier: {Attributes} = Initializer;
     *
     *      In the first case, the terminator can be the assignment operator ('=')
     *      In the second case, the terminator can be the decoration block terminator ('}')
     *
     *  Struct attributes:
     *      struct Identifier: Attributes {};
     *      OR
     *      struct Identifier: { Attributes } {};
     *
     *
     *  Annotation (macro) attributes:
     *      @Identifier: Attributes;
     *      OR
     *      @Identifier: { Attributes }
     *      OR
     *      @Identifier(): Attributes;
     *
     */

    struct Module : ASTNode
    {
        std::vector<ASTNode *> TopLevelNodes;
    };

    Lexer            &ObjectLexer;
    ArenaAllocator   &ObjectArenaAllocator;
    DiagnosticEngine &ObjectDiagnosticEngine;

    Token CurrentToken;
    Token PeekToken;

    Parser(const Parser &)            = default;
    Parser(Parser &&)                 = default;
    Parser &operator=(const Parser &) = delete;
    Parser &operator=(Parser &&)      = delete;
    Parser(Lexer &ParameterLexer, ArenaAllocator &ParameterArenaAllocator)
        : ObjectLexer(ParameterLexer), ObjectArenaAllocator(ParameterArenaAllocator),
          ObjectDiagnosticEngine(ParameterLexer.ObjectDiagnosticEngine)
    {
        advance_one_token();
        advance_one_token();
    }

    template <typename Type>
    void copy_vector_to_arena_allocated_span(std::vector<Type> &FromVector, std::span<Type> &ToSpan)
    {
        ToSpan = ObjectArenaAllocator.AllocateArray<Type>(FromVector.size());

        std::ranges::copy(FromVector, ToSpan.begin());
    }
    template <typename Type>
    std::span<Type> copy_vector_to_arena_allocated_span(std::vector<Type> &FromVector)
    {
        std::span<Type> to_span;
        copy_vector_to_arena_allocated_span(FromVector, to_span);

        return to_span;
    }

    template <ErrorCodes ErrorCode>
    void
    report_error_about_current_token(std::string Message, std::string Monologue, std::string Hint)
    {
        ObjectDiagnosticEngine.Report(
            Severity::Error,
            ErrorCode,
            CurrentToken.ObjectSourceLocation,
            std::move(Message),
            std::move(Monologue),
            std::move(Hint));
    }

    template <ErrorCodes ErrorCode>
    void report_error_about_current_token(std::string Message, std::string Monologue)
    {
        ObjectDiagnosticEngine.Report(
            Severity::Error,
            ErrorCode,
            CurrentToken.ObjectSourceLocation,
            std::move(Message),
            std::move(Monologue));
    }

    Token advance_one_token()
    {
        Token old_token = CurrentToken;
        if (old_token.TokenType == TokenTypes::EndOfFile)
        {
            return old_token;
        }

        CurrentToken = PeekToken;
        PeekToken    = ObjectLexer.GetNextToken();

        return old_token;
    }

    [[nodiscard]] bool check_token_type_of_peek_token(TokenTypes ExpectedTokenType) const
    {
        return PeekToken.TokenType == ExpectedTokenType;
    }

    [[nodiscard]] bool check_token_type_of_current_token(TokenTypes ExpectedTokenType) const
    {
        return CurrentToken.TokenType == ExpectedTokenType;
    }

    bool match_with_current_token(TokenTypes TokenType)
    {
        if (check_token_type_of_current_token(TokenType))
        {
            advance_one_token();
            return true;
        }

        return false;
    }

    Token expect_token_with_type(
        TokenTypes  TokenType,
        std::string ErrorMessage,
        std::string Monologue,
        std::string Hint      = "",
        ErrorCodes  ErrorCode = UNEXPECTED_TYPE)
    {

        if (check_token_type_of_current_token(TokenType))
        {
            return advance_one_token();
        }

        ObjectDiagnosticEngine.Report(
            Severity::Error,
            ErrorCode,
            CurrentToken.ObjectSourceLocation,
            std::move(ErrorMessage),
            std::move(Monologue),
            std::move(Hint));
        return {};
    }

    template <TokenTypes TokenType>
    Token expect_token_with_type(
        std::string ErrorMessage,
        std::string Monologue,
        std::string Hint      = "",
        ErrorCodes  ErrorCode = UNEXPECTED_TYPE)
    {

        if (check_token_type_of_current_token(TokenType))
        {
            return advance_one_token();
        }

        ObjectDiagnosticEngine.Report(
            Severity::Error,
            ErrorCode,
            CurrentToken.ObjectSourceLocation,
            std::move(ErrorMessage),
            std::move(Monologue),
            std::move(Hint));
        return {};
    }

    void expect_semicolon()
    {
        expect_token_with_type(
            TokenTypes::Semicolon,
            "Expected ';'.",
            "Programming language 101: USE YER DAMN SEMICOLONS!",
            "",
            EXPECTED_SEMICOLON);
    }

    // skipping all the fancy names, it's just parsing blocks like { stuff1, stuff2, stuff3 } or (
    // stuff1, stuff2, stuff3 ) and such
    template <
        ErrorCodes ErrorCode,
        bool       ConsumeInitializer  = false,
        bool       ConsumeTerminator   = false,
        typename CallbackFunctionType1 = bool (*)(),
        typename CallbackFunctionType2 = bool (*)()>
    std::span<ASTNode *> ParseArgumentativeExpressionUntilTerminator(
        std::string             ExpectedTerminatorErrorMessage,
        std::string             ExpectedTerminatorMonologue,
        CallbackFunctionType1 &&CallbackIsCurrentTokenTerminator,
        CallbackFunctionType2 &&CallbackIsCurrentTokenExpressionParserTerminator = []()
        { return false; })
    {
        // an expression might be like var value : { arg, arg2 = {arg3, .arg4 = 41} } = val;
        if constexpr (ConsumeInitializer)
        {
            advance_one_token();
        }

        std::vector<ASTNode *> temporary_ast_node_pointer_vector;
        while (!CallbackIsCurrentTokenTerminator() &&
               !check_token_type_of_current_token(TokenTypes::EndOfFile))
        {

            auto *ast_node = ParseExpression(0, CallbackIsCurrentTokenExpressionParserTerminator);
            if (ast_node != nullptr)
            {
                temporary_ast_node_pointer_vector.push_back(ast_node);
            }

            if (!match_with_current_token(TokenTypes::Comma))
            {
                break;
            }
        }

        if (!CallbackIsCurrentTokenTerminator())
        {
            ObjectDiagnosticEngine.Report(
                Severity::Error,
                ErrorCode,
                CurrentToken.ObjectSourceLocation,
                std::move(ExpectedTerminatorErrorMessage),
                std::move(ExpectedTerminatorMonologue));
        }
        else if constexpr (ConsumeTerminator)
        {
            if (!check_token_type_of_current_token(TokenTypes::EndOfFile))
            {
                advance_one_token();
            }
        }

        return copy_vector_to_arena_allocated_span(temporary_ast_node_pointer_vector);
    }

    std::span<ASTNode *> ParseFunctionArguments()
    {
        // (arg1, arg2, arg3)
        return ParseArgumentativeExpressionUntilTerminator<EXPECTED_RIGHT_PARENTHESIS, true, true>(
            "Expected ')' after '('.",
            "Lord... it's a wonder you got so far with your wits, mister. Close your damn '(' with "
            "a ')'.",
            [this]() { return check_token_type_of_current_token(TokenTypes::RightParenthesis); });
    }

    ASTNode *ParseFunctionCallExpression()
    {
        auto *function_call_expression_node = ObjectArenaAllocator.Allocate<FunctionCallExpression>(
            CurrentToken.ObjectSourceLocation);

        function_call_expression_node->Arguments = ParseFunctionArguments();

        return function_call_expression_node;
    };
    ASTNode *ParseFunctionCallExpression(SourceLocation ParameterSourceLocation)
    {
        auto *function_call_expression_node =
            ObjectArenaAllocator.Allocate<FunctionCallExpression>(ParameterSourceLocation);

        function_call_expression_node->Arguments = ParseFunctionArguments();

        return function_call_expression_node;
    };

    ASTNode *ParseIfExpression()
    {
        /*
         * if() {
         *
         * } else {
         *
         * }
         */

        auto start_location = CurrentToken.ObjectSourceLocation;
        advance_one_token(); // consume 'if'

        auto *if_expression_node = ObjectArenaAllocator.Allocate<IfExpression>(start_location);

        if (match_with_current_token(TokenTypes::Colon))
        {
            if_expression_node->EvaluateAtCompileTime = true;
        }

        if_expression_node->Condition = ParseExpression(0);

        if (check_token_type_of_current_token(TokenTypes::LeftBrace))
        {
            // if () { ... }
            if_expression_node->ThenBranch = ParseStatement();
        }
        else
        {
            // if () expression
            if_expression_node->ThenBranch = ParseExpression(0);
        }

        if (match_with_current_token(TokenTypes::Keyword_Else))
        {
            if (check_token_type_of_current_token(TokenTypes::Keyword_If))
            {
                // else if
                if_expression_node->ElseBranch = ParseIfExpression();
            }
            else if (check_token_type_of_current_token(TokenTypes::LeftBrace))
            {
                // else {}
                if_expression_node->ElseBranch = ParseBlockStatement();
            }
            else
            {
                // else expression
                if_expression_node->ElseBranch = ParseExpression(0);
            }
        }

        return if_expression_node;
    };

    ASTNode *ParseImplicitAccessExpression(
        SourceLocation   SourceLocationOfDotOperator,
        std::string_view SourceOfTargetMember)
    {
        return ObjectArenaAllocator.Allocate<ImplicitMemberAccessExpression>(
            SourceLocationOfDotOperator, SourceOfTargetMember);
    }

    static uint32_t get_operator_precedence(TokenTypes TokenType)
    {
        switch (TokenType)
        {
        case TokenTypes::Equal:
        {
            return 5;
        }
        case TokenTypes::Plus:
        case TokenTypes::PlusPlus:
        case TokenTypes::Minus:
        case TokenTypes::MinusMinus:
        {
            return 10;
        }
        case TokenTypes::Star:
        case TokenTypes::Slash:
        case TokenTypes::Not:
        case TokenTypes::AND:
        case TokenTypes::XOR:
        case TokenTypes::OR:
        case TokenTypes::NotEqual:
        case TokenTypes::LessThanOrEqualTo:
        case TokenTypes::GreaterThanOrEqualTo:
        case TokenTypes::EqualEqual:
        {
            return 20;
        }
        case TokenTypes::Dot:
        case TokenTypes::LeftParenthesis:
        case TokenTypes::LeftBrace:
        {
            return 30;
        }
        default:
        {
            return 0;
        }
        }
    }

    ASTNode *ParsePrimaryExpression()
    {
        SourceLocation start_location = CurrentToken.ObjectSourceLocation;

        switch (CurrentToken.TokenType)
        {
        case TokenTypes::IntegerLiteral:
        case TokenTypes::FloatLiteral:
        case TokenTypes::StringLiteral:
        case TokenTypes::CharacterLiteral:
        {
            advance_one_token();
            return ObjectArenaAllocator.Allocate<LiteralExpression>(start_location);
        }
        case TokenTypes::Identifier:
        {
            advance_one_token();

            // std::cout << CurrentToken.ObjectSourceLocation.Source << "\n"
            //           << PeekToken.ObjectSourceLocation.Source << " <<\n";
            return ObjectArenaAllocator.Allocate<IdentifierExpression>(start_location);
        }
        case TokenTypes::LeftParenthesis:
        {
            advance_one_token();
            ASTNode *expression = ParseExpression(0);
            expect_token_with_type(
                TokenTypes::RightParenthesis,
                "Expected ')' after parenthesized expression.",
                "Mister, you... You ain't the brightest tool in the shed, are ya? Close your damn "
                "'(' with a ')'!",
                "Close '(' with ')'.",
                EXPECTED_RIGHT_PARENTHESIS);

            return expression;
        }
        case TokenTypes::LeftBrace:
        {
            advance_one_token();
            ASTNode *expression = ParseExpression(0);
            expect_token_with_type(
                TokenTypes::RightBrace,
                "Expected '}' after braced expression.",
                "Mister, you... You ain't the brightest tool in the shed, are ya? Close yer damn "
                "'{' with a '}'!",
                "Close '{' with '}'.",
                EXPECTED_RIGHT_BRACE);

            return expression;
        }
        case TokenTypes::Minus:
        case TokenTypes::MinusMinus:
        case TokenTypes::Plus:
        case TokenTypes::PlusPlus:
        case TokenTypes::Not:
        {
            auto               operand_token_type = advance_one_token().TokenType;
            constexpr uint32_t PREFIX_PRECEDENCE  = 40;
            auto              *identifier         = ParseExpression(PREFIX_PRECEDENCE);

            advance_one_token();
            return ObjectArenaAllocator.Allocate<UnaryExpression>(
                start_location, operand_token_type, identifier);
        }
        case TokenTypes::Keyword_If:
        {
            return ParseIfExpression();
        }
        case TokenTypes::Keyword_Switch:
        {
            return ParseSwitchExpression();
        }
        case TokenTypes::Dot:
        {
            if (!check_token_type_of_peek_token(TokenTypes::Identifier))
            {
                advance_one_token();
                report_error_about_current_token<EXPECTED_IDENTIFIER>(
                    "Expected an identifier after implicit member access operator '.' (e.g., "
                    "'.Member').",
                    "Lord. I don't even have words for this.... Ugh- Place a darn word after your "
                    "'.', "
                    "mister.");

                return nullptr;
            }

            return ParseImplicitAccessExpression(
                advance_one_token().ObjectSourceLocation,
                advance_one_token().ObjectSourceLocation.Source);
        }
        default:
        {
            break;
        }
        }

        ObjectDiagnosticEngine.Report(
            Severity::Error,
            UNEXPECTED_EXPRESSION_TOKEN,
            start_location,
            "Unexpected expression token.",
            "Now, I ain't know what you damn wrote, but it's damn idiotic, I tell ya...");
        return nullptr;
    };

    template <typename CallbackFunctionType = bool (*)()>
    /*  @brief rough example:
     *  Given: 1 + 2 * 3
     *  lhs = 1; consume 1
     *
     *  operator = '+'; consume '+' // cursor position = 2 * 3
     *
     *  rhs =  --- recurse(1's precedence + 1)
     *      lhs = 2 : 2's precedence = 0; consume 2 // cursor position * 3
     *      operator = '*' : precedence = 30 // cursor position 3
     *
     *      rhs = --- recurse(precedence + 1)
     *          lhs = 3 : 3's precedence = 0; consume 3 // cursor position
     *      ---
     *      return { '*' '2' '3' }
     *  ---
     *
     *  return {'+' '1' {'*' '2' '3'} }
     *
     *  Another example:
     *  a + b * f(c, .d = 214)
     *  lhs = a
     *  operator = + : 10
     *  rhs = (11) {
     *      lhs = b
     *      operator = * : 20
     *      rhs = (21) {
     *          lhs = f;
     *          operator = ( : 30
     *          f is a function since identifier -> left parenthesis pattern
     *          lhs = parse_func_args()
     * }
     * }
     *
     * parse_func_args() {
     *  lhs = c
     *  operator = ,
     *  append('c')
     *  ---
     *  lhs = .
     *  '.' is a prefix operator, and it is a dot operator. Most likely an implicit value access
     *  lhs = {'.' 'd'}
     *  operator = '=' : 5
     *  rhs = (6) {
     *      lhs = 214
     *      operator = ')' : 0
     *      return 214
     * }
     * }
     *
     * final output = {'+' 'a' { '*' 'b' {({'c', {'=' {'.' 'd'} '214'}) 'f' }}
     */
    ASTNode *ParseExpression(
        uint32_t               MinimumPrecedence    = 0,
        CallbackFunctionType &&CallbackIsTerminator = []() { return false; })
    {
        /*
         *  lhs = inline; precedence 0
         *  operand = comma; precedence 0
         *  create singular node and return {inline}
         */

        auto *left_hand_side = ParsePrimaryExpression();

        if (left_hand_side == nullptr)
        {
            return left_hand_side;
        }

        while (true)
        {
            switch (CurrentToken.TokenType)
            {
            case TokenTypes::RightSquareBracket:
            case TokenTypes::RightAngleBrace:
            case TokenTypes::RightParenthesis:
            case TokenTypes::RightBrace:
            case TokenTypes::Comma:
            case TokenTypes::Semicolon:
            {
                return left_hand_side;
            }
            default:
            {
                if (CallbackIsTerminator())
                {
                    return left_hand_side;
                }

                break;
            }
            }

            uint32_t precedence = get_operator_precedence(CurrentToken.TokenType);
            if (precedence < MinimumPrecedence ||
                check_token_type_of_current_token(TokenTypes::EndOfFile))
            {
                break;
            }

            Token operator_token = advance_one_token();

            switch (operator_token.TokenType)
            {
            case TokenTypes::LeftParenthesis:
            {
                if (left_hand_side != nullptr &&
                    left_hand_side->NodeType == NodeTypes::IdentifierExpression)
                {
                    left_hand_side =
                        ParseFunctionCallExpression(left_hand_side->ObjectSourceLocation);
                }
                break;
            }
            case TokenTypes::Dot:
            {
                auto target_member =
                    expect_token_with_type(
                        TokenTypes::Identifier,
                        "Expected identifier after access operator ('.').",
                        "I've seen things, mister. But never, even from Micah, have I seen such "
                        "idiocy. PUT A DAMN WORD OR SOMETHIN' AFTER YOUR '.'! Expect me to read "
                        "your "
                        "damn mind "
                        "otherwise?!",
                        "",
                        EXPECTED_IDENTIFIER)
                        .ObjectSourceLocation.Source;

                if (target_member.empty())
                {
                    return left_hand_side;
                }

                if (left_hand_side != nullptr &&
                    left_hand_side->NodeType == NodeTypes::IdentifierExpression)
                {
                    left_hand_side = ObjectArenaAllocator.Allocate<ExplicitMemberAccessExpression>(
                        operator_token.ObjectSourceLocation, left_hand_side, target_member);

                    break;
                }

                left_hand_side = ObjectArenaAllocator.Allocate<ImplicitMemberAccessExpression>(
                    operator_token.ObjectSourceLocation, target_member);
                break;
            }
            default:
            {
                auto *right_hand_side = ParseExpression(precedence + 1);

                if (right_hand_side == nullptr)
                {
                    return left_hand_side;
                }

                left_hand_side = ObjectArenaAllocator.Allocate<BinaryExpression>(
                    operator_token.TokenType,
                    left_hand_side,
                    right_hand_side,
                    operator_token.ObjectSourceLocation);
            }
            }
        }

        return left_hand_side;
    };

    template <bool ExpectSemicolon = true> std::span<ASTNode *> ParseVariableAttributes()
    {
        // var identifier: attributes = value;
        // struct identifier: attributes {};
        if (match_with_current_token(TokenTypes::LeftBrace))
        {
            return ParseArgumentativeExpressionUntilTerminator<
                EXPECTED_RIGHT_BRACE_OR_SEMICOLON,
                false,
                true>(
                "Expected '}' after ':'.",
                "Think I've seen bricks with more wit than you, mister. Place a damn '}' after "
                "':'!",
                [this]()
                {
                    switch (CurrentToken.TokenType)
                    {
                    case TokenTypes::RightBrace:
                    {
                        return true;
                    }
                    default:
                    {
                        return false;
                    }
                    }
                });
        }

        if constexpr (ExpectSemicolon)
        {
            return ParseArgumentativeExpressionUntilTerminator<EXPECTED_EQUAL_SIGN_OR_SEMICOLON>(
                "Expected '=' or ';' after attribute expression.",
                "Now, how'd you reckon I'm supposed to figure out where your damned code "
                "ends? Give me a damned semicolon or an equal sign, or take your ugly mug "
                "somewhere "
                "else, mister.",
                [this]()
                {
                    switch (CurrentToken.TokenType)
                    {
                    case TokenTypes::Semicolon:
                    case TokenTypes::Equal:
                    {
                        return true;
                    }
                    default:
                    {
                        return false;
                    }
                    }
                },
                [this]() { return check_token_type_of_current_token(TokenTypes::Equal); });
        }

        if (!check_token_type_of_current_token(TokenTypes::Identifier))
        {
            report_error_about_current_token<EXPECTED_IDENTIFIER>(
                "Expected identifier to start attribute.", "");
            return {};
        }

        auto  singular_attribute_span = ObjectArenaAllocator.AllocateArray<ASTNode *>(1);
        auto *singular_attribute      = ParseExpression();

        return std::move(singular_attribute, singular_attribute_span);
    }

    template <
        bool IsImmutableVariable          = false,
        bool UseDeclaratorAsStartLocation = true,
        bool ExpectSemicolon              = true>
    ASTNode *ParseVariableDeclarationStatement()
    {
        /*
         * var Something: Type;
         * var Something = Expression;
         * var Something: Type, Flags = Expression;
         * var Something: {Type, Flag1 = .SomeValue} = Expression;
         *
         * const Something: Type;
         *
         * (Something: Type) as the rest
         *
         * { struct like
         *  Something: Type;
         *  Something = Expression;
         * }
         */

        // yeah, does a lot of things at once. but that allows later passes to have simpler
        // introspection logic as well, as every one of these declarations have the exact same
        // output node

        SourceLocation start_location;

        if constexpr (UseDeclaratorAsStartLocation)
        {
            start_location = advance_one_token().ObjectSourceLocation; // consume 'var/const'
        }
        else
        {
            start_location = CurrentToken.ObjectSourceLocation;
        }

        auto *variable_declaration_node =
            ObjectArenaAllocator.Allocate<VariableDeclarationStatement>(start_location);

        if constexpr (IsImmutableVariable)
        {
            variable_declaration_node->IsImmutable = true;
        }

        variable_declaration_node->VariableName =
            expect_token_with_type(
                TokenTypes::Identifier,
                "Expected identifier after variable declaration statement.",
                "I've seen morons, mister. But I don't think none of em are as dull as you are. "
                "Put a damned word or somethin' after your 'var' or 'const'!")
                .ObjectSourceLocation.Source;

        ASTNode *initializer = nullptr;

        if (match_with_current_token(TokenTypes::Colon))
        {
            variable_declaration_node->Attributes = ParseVariableAttributes<ExpectSemicolon>();
        }

        if (match_with_current_token(TokenTypes::Equal))
        {
            initializer = ParseExpression(0);
        }

        if constexpr (ExpectSemicolon)
        {
            expect_semicolon();
        }

        if (initializer != nullptr)
        {
            variable_declaration_node->Initializer = initializer;
        }

        return variable_declaration_node;
    }

    // expected input: { decoration1, decor2, decor3 }
    std::span<ASTNode *> ParseAttributes()
    {
        if (match_with_current_token(TokenTypes::LeftBrace))
        {
            return ParseArgumentativeExpressionUntilTerminator<EXPECTED_RIGHT_BRACE, false, true>(
                "Expected '}' after '{'.",
                "Lord... it's a wonder you got so far with your wits, mister. Close your damn '{' "
                "with "
                "a '}'.",
                [this]() { return check_token_type_of_current_token(TokenTypes::RightBrace); });
        }

        auto expression = ParseArgumentativeExpressionUntilTerminator<
            EXPECTED_SEMICOLON,
            false,
            true>(
            "Expected terminator ';' after annotation (macro) declaration.",
            "How do you think I'm supposed to know when your damn code ends? You thinkin' I'm a "
            "magician, mister? Put a damn semicolon (';') after your statement.",
            [this]() { return check_token_type_of_current_token(TokenTypes::Semicolon); });

        return expression;
    }

    ASTNode *ParseAnnotatedNode()
    {
        auto start_location = advance_one_token().ObjectSourceLocation; // consume '@'

        auto *annotated_node =
            ObjectArenaAllocator.Allocate<AnnotationFunctionExpression>(start_location);

        annotated_node->Identifier =
            expect_token_with_type(
                TokenTypes::Identifier,
                "Expected identifier after annotation declarator '@'.",
                "Mister, I've met idiots left and right in my time, but you might just be the "
                "winner. "
                "How do you reckon I'm supposed to track your annotated functions?")
                .ObjectSourceLocation.Source;

        if (annotated_node->Identifier.empty())
        {
            return annotated_node;
        }

        if (check_token_type_of_current_token(TokenTypes::LeftParenthesis))
        {
            annotated_node->Arguments = ParseFunctionArguments();
        }

        if (match_with_current_token(TokenTypes::Colon))
        {
            annotated_node->Decorations = ParseAttributes();
        }

        return annotated_node;
    }

    /// single parameter
    ASTNode *ParseFunctionDeclarationParameter()
    {
        /*
         * (param Something: Type, Flags)
         * (param Something: { Type, Flags } )
         * (param Something: Type, Flags = DefaultValue )
         * (param Something: Type, )
         */
    }

    template <bool ConsumeInitializer = true, bool ConsumeTerminator = true>
    std::span<ASTNode *> ParseFunctionDeclarationParameters()
    {
        // Expect to parse (Param1: Attributes, Param2: Attributes)
        if constexpr (ConsumeInitializer)
        {
            advance_one_token();
        }

        if (CurrentToken.TokenType == TokenTypes::RightParenthesis)
        {
            return {};
        }

        std::vector<ASTNode *> temporary_parameter_pointer_vector;
        while (!check_token_type_of_current_token(TokenTypes::RightParenthesis) &&
               !check_token_type_of_peek_token(TokenTypes::EndOfFile))
        {
            if (!check_token_type_of_current_token(TokenTypes::Identifier))
            {
                report_error_about_current_token<EXPECTED_IDENTIFIER>(
                    "Whilst parsing for function parameters, found unexpected token.", "");
                break;
            }

            // is not immutable, does not have a declarator, and doesn't expect a semicolon as a
            // terminator
            auto *parameter = ParseVariableDeclarationStatement<false, false, false>();

            temporary_parameter_pointer_vector.push_back(parameter);
            advance_one_token();

            if (!match_with_current_token(TokenTypes::Comma))
            {
                break;
            }
        }

        if constexpr (ConsumeTerminator)
        {
            advance_one_token(); // consume ')'
        }
        return copy_vector_to_arena_allocated_span(temporary_parameter_pointer_vector);
    }

    /// doesn't consume trailing semicolons
    /// expected input: { attributes } OR attributes
    std::span<ASTNode *> ParseStructOrFunctionAttributes()
    {
        if (match_with_current_token(TokenTypes::LeftBrace))
        {
            return ParseArgumentativeExpressionUntilTerminator<EXPECTED_RIGHT_BRACE, false, true>(
                "Expected terminator '}' after '{'.",
                "",
                [this]() { return check_token_type_of_current_token(TokenTypes::RightBrace); });
        }

        auto parsed_function_attributes =
            ParseArgumentativeExpressionUntilTerminator<EXPECTED_LEFT_BRACE>(
                "Expected terminator '{' after ':'.",
                "",
                [this]() { return check_token_type_of_current_token(TokenTypes::LeftBrace); },
                [this]() -> bool
                { return check_token_type_of_current_token(TokenTypes::LeftBrace); });

        return parsed_function_attributes;
    }

    ASTNode *ParseReturnStatement()
    {
        // return expression
        auto start_location = CurrentToken.ObjectSourceLocation;
        advance_one_token(); // consume return

        auto *return_statement = ObjectArenaAllocator.Allocate<ReturnStatement>(start_location);

        return_statement->Expression = ParseExpression(0);
        expect_semicolon();

        return return_statement;
    }

    template <NodeTypes StatementNodeType> ASTNode *ParseGenericSingularStatement()
    {
        auto *generic_statement_node = ObjectArenaAllocator.Allocate<GenericStatement>(
            CurrentToken.ObjectSourceLocation, StatementNodeType);
        advance_one_token(); // consume the statement

        return generic_statement_node;
    }

    ASTNode *ParseStatement()
    {
        switch (CurrentToken.TokenType)
        {
        case TokenTypes::Keyword_MutableVariable:
        case TokenTypes::Keyword_Constant:
            return ParseVariableDeclarationStatement();
        case TokenTypes::Keyword_If:
            return ParseIfExpression();
        case TokenTypes::Keyword_Return:
            return ParseReturnStatement();
        case TokenTypes::LeftBrace:
            return ParseBlockStatement();
        case TokenTypes::Keyword_Continue:
            return ParseGenericSingularStatement<NodeTypes::ContinueStatement>();
        case TokenTypes::Keyword_Break:
            return ParseGenericSingularStatement<NodeTypes::BreakStatement>();
        default:
        {
            auto     start_location       = CurrentToken.ObjectSourceLocation;
            ASTNode *expression_statement = ParseExpression(0);

            expect_semicolon();
            return ObjectArenaAllocator.Allocate<ExpressionStatement>(
                start_location, expression_statement);
        }
        }
    }

    /// Unlike attribute parsers, this consumes the initializer regardless to keep track of the
    /// starting location of the block statement.
    ASTNode *ParseBlockStatement()
    {
        auto start_location = advance_one_token().ObjectSourceLocation; // consume '{'

        std::vector<ASTNode *> temporary_statement_pointer_vector;
        while (!check_token_type_of_current_token(TokenTypes::RightBrace) &&
               !check_token_type_of_peek_token(TokenTypes::EndOfFile))
        {
            auto *statement = ParseStatement();
            if (statement != nullptr)
            {
                temporary_statement_pointer_vector.push_back(statement);
            }
        }

        expect_token_with_type(
            TokenTypes::RightBrace,
            "Expected '}' to start block statement.",
            "Who left this idiot here? Oh, calm down, mister, I’m joking... You’re not an idiot, "
            "you’re a moron. Because your wit hasn't lead you to figuring out that a function "
            "ends with a damned '}'.");

        return ObjectArenaAllocator.Allocate<BlockStatement>(
            start_location,
            copy_vector_to_arena_allocated_span(temporary_statement_pointer_vector));
    }

    ASTNode *ParseFunctionDeclaration()
    {
        /*
         * e.g.,
         * fn Identifier(Params: Attributes) : Attributes
         * {
         *
         * }
         *
         * fn Identifier(Params: Attributes) -> TypeName : Attributes
         * {
         *
         * }
         *
         * fn Identifier(Params: Attributes) -> TypeName : { Attributes }
         * {
         *
         * }
         */

        auto start_location = advance_one_token().ObjectSourceLocation; // consume "fn"

        auto *function_declaration_statement =
            ObjectArenaAllocator.Allocate<FunctionDeclarationStatement>(start_location);

        function_declaration_statement->Identifier =
            expect_token_with_type(
                TokenTypes::Identifier,
                "Expected identifier after function declaration statement.",
                "Mister, I'm getting tired of this. Just place a damned word after your function "
                "declaration statement or somethin'.")
                .ObjectSourceLocation.Source;

        if (function_declaration_statement->Identifier.empty())
        {
            return function_declaration_statement;
        }

        if (match_with_current_token(TokenTypes::LeftParenthesis))
        {
            function_declaration_statement->Parameters =
                ParseFunctionDeclarationParameters<false>();
        }

        if (match_with_current_token(TokenTypes::RightArrow))
        {
            function_declaration_statement->ReturnType =
                expect_token_with_type(
                    TokenTypes::Identifier,
                    "Expected identifier after return type declarator (->).",
                    "")
                    .ObjectSourceLocation.Source;
        }

        if (match_with_current_token(TokenTypes::Colon))
        {
            function_declaration_statement->Attributes = ParseStructOrFunctionAttributes();
        }

        if (match_with_current_token(TokenTypes::Semicolon)) // forward decl probably
        {
            return function_declaration_statement;
        };

        if (check_token_type_of_current_token(TokenTypes::LeftBrace))
        {
            function_declaration_statement->FunctionBody = ParseBlockStatement();
            return function_declaration_statement;
        }

        expect_token_with_type(
            TokenTypes::LeftBrace,
            "Expected '{' after function declaration.",
            "Lord... you're dumber than I thought you would be. That is, dumber than a damn rock. "
            "Place a damned ';' or '{' after your function declaration.");

        return function_declaration_statement;
    };

    std::span<ASTNode *> ParseCaseExpression()
    {
        if (check_token_type_of_current_token(TokenTypes::LeftParenthesis))
        {
            return ParseArgumentativeExpressionUntilTerminator<EXPECTED_LEFT_PARENTHESIS, true>(
                "Expected ')' after case expression.",
                "",
                [this]()
                { return check_token_type_of_current_token(TokenTypes::RightParenthesis); });
        }

        return ParseArgumentativeExpressionUntilTerminator<EXPECTED_LEFT_PARENTHESIS>(
            "Expected '->' after case expression.",
            "",
            [this]() { return check_token_type_of_current_token(TokenTypes::RightArrow); });
    }

    ASTNode *ParseSwitchExpression()
    {
        /*
         * switch () {
         *  case cond, cond1 -> expression;
         * }
         *
         * switch () {
         *  case cond -> {
         *      statements;
         *  }
         * }
         */

        auto start_location = advance_one_token().ObjectSourceLocation; // consume 'switch'

        auto *switch_expression_node =
            ObjectArenaAllocator.Allocate<SwitchExpression>(start_location);

        switch_expression_node->Condition = ParseExpression(0);

        if (!match_with_current_token(TokenTypes::LeftBrace))
        {
            ObjectDiagnosticEngine.Report(
                Severity::Error,
                EXPECTED_LEFT_BRACE,
                CurrentToken.ObjectSourceLocation,
                "Expected '{' after switch expression.",
                "");
            return switch_expression_node;
        }

        advance_one_token(); // consume '{'

        std::vector<SwitchExpression::Case *> temporary_cases_pointer_vector;
        while (!check_token_type_of_current_token(TokenTypes::RightBrace) ||
               check_token_type_of_peek_token(TokenTypes::EndOfFile))
        {
            switch (CurrentToken.TokenType)
            {
            case TokenTypes::Keyword_Default:
            case TokenTypes::Keyword_Case:
            {
                auto *switch_case_expression =
                    ObjectArenaAllocator.Allocate<SwitchExpression::Case>();
                switch_case_expression->ObjectCaseSourceLocation =
                    CurrentToken.ObjectSourceLocation;

                if (advance_one_token().TokenType == TokenTypes::Keyword_Case)
                {
                    switch_case_expression->ForCondition = ParseCaseExpression();
                }

                if (!check_token_type_of_current_token(TokenTypes::RightArrow))
                {
                    ObjectDiagnosticEngine.Report(
                        Severity::Error,
                        EXPECTED_RIGHT_ARROW,
                        CurrentToken.ObjectSourceLocation,
                        "Expected '->' after switch case expression.",
                        "");
                    advance_one_token(); // consume invalid token
                    continue;
                }

                advance_one_token(); // consume '->'

                if (check_token_type_of_current_token(TokenTypes::LeftBrace))
                {
                    switch_case_expression->ThenExpression = ParseBlockStatement();
                }
                else
                {
                    switch_case_expression->ThenExpression = ParseExpression(0);
                }

                temporary_cases_pointer_vector.push_back(switch_case_expression);
                expect_token_with_type(
                    TokenTypes::Comma, "Expected ',' after case expression.", "");
                break;
            }
            case TokenTypes::EndOfFile:
            {
                report_error_about_current_token<UNEXPECTED_END_OF_FILE>(
                    "Found unexpected end of file whilst parsing for a switch case expression.",
                    "");
                return switch_expression_node;
            }
            default:
            {
                report_error_about_current_token<UNEXPECTED_EXPRESSION_TOKEN>(
                    "Unexpected token.", "");
                advance_one_token();
                break;
            }
            }
        }

        expect_token_with_type(
            TokenTypes::RightBrace,
            "Expected '{' to terminate switch statement.",
            "Mister, you... You're lucky I'm in a good mood today. Just damn close your switch "
            "statement with a '{', will you?");

        switch_expression_node->Cases =
            copy_vector_to_arena_allocated_span(temporary_cases_pointer_vector);

        return switch_expression_node;
    }

    std::span<ASTNode *> ParseForStatementCaptures()
    {
        return ParseArgumentativeExpressionUntilTerminator<EXPECTED_PIPE_SYMBOL, true, true>(
            "Expected '|' after capture initialization.",
            "",
            [this]() { return check_token_type_of_current_token(TokenTypes::Pipe); });
    }

    ASTNode *ParseForStatement()
    {
        /*
         * for () | | {}
         *
         */

        auto  start_location     = CurrentToken.ObjectSourceLocation;
        auto *for_statement_node = ObjectArenaAllocator.Allocate<ForStatement>(start_location);

        advance_one_token(); // consume 'for'

        for_statement_node->Condition = ParseExpression(0);

        if (check_token_type_of_current_token(TokenTypes::Pipe))
        {
            for_statement_node->Captures = ParseForStatementCaptures();
        }

        if (!check_token_type_of_current_token(TokenTypes::RightBrace))
        {
            report_error_about_current_token<EXPECTED_RIGHT_BRACE>(
                "Expected '{' after for loop statement.", "");
            return for_statement_node;
        }

        for_statement_node->BlockStatement = ParseBlockStatement();

        return for_statement_node;
    }

    ASTNode *ParseWhileStatement()
    {
        /*
         * while () | | {}
         *
         */

        auto  start_location     = CurrentToken.ObjectSourceLocation;
        auto *for_statement_node = ObjectArenaAllocator.Allocate<WhileStatement>(start_location);

        advance_one_token(); // consume 'while'

        for_statement_node->Condition = ParseExpression(0);

        if (!check_token_type_of_current_token(TokenTypes::RightBrace))
        {
            report_error_about_current_token<EXPECTED_RIGHT_BRACE>(
                "Expected '{' after while loop statement.", "");
            return for_statement_node;
        }

        for_statement_node->BlockStatement = ParseBlockStatement();

        return for_statement_node;
    }

    ASTNode *ParseImportStatement()
    {
        /*
         * import something/something as something;
         * import something as unsafe;
         */

        auto start_location = advance_one_token().ObjectSourceLocation; // consume "import"

        auto *import_statement_node =
            ObjectArenaAllocator.Allocate<ImportStatement>(start_location);

        if (!check_token_type_of_current_token(TokenTypes::Identifier))
        {
            report_error_about_current_token<UNEXPECTED_TOKEN>(
                "Unexpected token found whilst parsing for import statement.", "");
            return import_statement_node;
        }

        std::string_view starting_import_from_identifier =
            advance_one_token().ObjectSourceLocation.Source;

        std::string_view ending_import_from_identifier;

        auto assign_import_identifier = [&]() -> void
        {
            // end - start
            auto total_merged_slice_length = static_cast<std::size_t>(
                (ending_import_from_identifier.data() + ending_import_from_identifier.size()) -
                starting_import_from_identifier.data());

            // from start to length
            std::string_view full_identifier(
                starting_import_from_identifier.data(), total_merged_slice_length); // NOLINT
            import_statement_node->ImportFrom = full_identifier;
        };

        while (true)
        {
            switch (CurrentToken.TokenType)
            {
            case TokenTypes::Keyword_As:
            {
                assign_import_identifier();

                advance_one_token(); // consume "as"

                if (!check_token_type_of_current_token(TokenTypes::Identifier) &&
                    !check_token_type_of_current_token(TokenTypes::Keyword_Unsafe))
                {
                    goto ExpectSemicolonAndReturnNode;
                }

                if (check_token_type_of_current_token(TokenTypes::Identifier))
                {
                    import_statement_node->ImportAs =
                        ObjectArenaAllocator.Allocate<IdentifierExpression>(
                            CurrentToken.ObjectSourceLocation);
                }
                else
                {
                    import_statement_node->ImportAs =
                        ObjectArenaAllocator.Allocate<GenericStatement>(
                            CurrentToken.ObjectSourceLocation, NodeTypes::UnsafeStatement);
                }

                advance_one_token();
                goto ExpectSemicolonAndReturnNode;
            }
            case TokenTypes::Semicolon:
            case TokenTypes::EndOfFile:
            {
                assign_import_identifier();
                goto ExpectSemicolonAndReturnNode;
            }
            default:
                break;
            }

            ending_import_from_identifier = advance_one_token().ObjectSourceLocation.Source;
        }

    ExpectSemicolonAndReturnNode:
    {
        expect_semicolon();
        return import_statement_node;
    }
    }

    ASTNode *ParseStructDeclarationStatement()
    {
        /*
         * struct Identifier: Type, Attributes = {}
         *
         */

        auto *struct_declaration_node = ObjectArenaAllocator.Allocate<StructDeclarationStatement>(
            advance_one_token().ObjectSourceLocation);

        struct_declaration_node->Identifier =
            expect_token_with_type<TokenTypes::Identifier>(
                "Expected identifier after struct declarator.", "")
                .ObjectSourceLocation.Source;

        if (struct_declaration_node->Identifier.empty())
        {
            return struct_declaration_node;
        }

        if (match_with_current_token(TokenTypes::Colon))
        {
            struct_declaration_node->Attributes = ParseStructOrFunctionAttributes();
        }

        if (!match_with_current_token(TokenTypes::LeftBrace))
        {
            report_error_about_current_token<EXPECTED_RIGHT_BRACE>(
                "Expected '{' whilst parsing for struct declaration.", "");
            return struct_declaration_node;
        }

        std::vector<ASTNode *> struct_declaration_implementation_vector;
        while (!check_token_type_of_current_token(TokenTypes::RightBrace) &&
               !check_token_type_of_current_token(TokenTypes::EndOfFile))
        {
            if (!check_token_type_of_current_token(TokenTypes::Identifier))
            {
                report_error_about_current_token<UNEXPECTED_TOKEN>(
                    "Found unexpected token whilst parsing struct implementation ({ ... }).", "");
                break;
            }

            struct_declaration_implementation_vector.push_back(
                ParseVariableDeclarationStatement<false, false>());
        }

        if (check_token_type_of_current_token(TokenTypes::EndOfFile))
        {
            report_error_about_current_token<UNEXPECTED_END_OF_FILE>("Unexpected end of file.", "");
            return struct_declaration_node;
        }
        advance_one_token();

        if (struct_declaration_implementation_vector.empty())
        {
            return struct_declaration_node;
        }

        copy_vector_to_arena_allocated_span(
            struct_declaration_implementation_vector,
            struct_declaration_node->StructImplementation);

        return struct_declaration_node;
    }

    Module *ParseModule()
    {
        auto *module = ObjectArenaAllocator.Allocate<Module>();

        auto report_and_consume_unexpected_token = [this]() -> void
        {
            ObjectDiagnosticEngine.Report(
                Severity::Error,
                UNRECOGNIZED_TOP_LEVEL_NODE,
                CurrentToken.ObjectSourceLocation,
                "Unrecognized top level token.",
                "You sure don't look like you'd get very far on your wits.");
            advance_one_token(); // consume unknown token.
        };

        while (!check_token_type_of_current_token(TokenTypes::EndOfFile))
        {
            std::print(
                "Parsing token: {} | TokenType: {} | PeekToken: {} | PeekTokenType: {} | Line: "
                "{}\n",
                CurrentToken.ObjectSourceLocation.Source,
                std::to_underlying(CurrentToken.TokenType),
                PeekToken.ObjectSourceLocation.Source,
                std::to_underlying(PeekToken.TokenType),
                CurrentToken.ObjectSourceLocation.Line);

            ObjectDiagnosticEngine.PrintBuffer();
            switch (CurrentToken.TokenType)
            {
            case TokenTypes::Keyword_Import:
            {
                module->TopLevelNodes.push_back(ParseImportStatement());
                break;
            }

            case TokenTypes::AtSymbol:
            {
                module->TopLevelNodes.push_back(ParseAnnotatedNode());
                break;
            }
            case TokenTypes::Keyword_MutableVariable:
            {
                module->TopLevelNodes.push_back(ParseVariableDeclarationStatement());
                break;
            }
            case TokenTypes::Keyword_Constant:
            {
                module->TopLevelNodes.push_back(ParseVariableDeclarationStatement<true>());
                break;
            }
            case TokenTypes::Keyword_Switch:
            {
                module->TopLevelNodes.push_back(ParseSwitchExpression());
                break;
            }
            case TokenTypes::Keyword_FunctionDeclaration:
            {
                module->TopLevelNodes.push_back(ParseFunctionDeclaration());
                break;
            }
            case TokenTypes::Keyword_For:
            {
                module->TopLevelNodes.push_back(ParseForStatement());
                break;
            }
            case TokenTypes::Keyword_While:
            {
                module->TopLevelNodes.push_back(ParseWhileStatement());
                break;
            }
            case TokenTypes::Identifier:
            {
                if (check_token_type_of_peek_token(TokenTypes::LeftParenthesis))
                {
                    module->TopLevelNodes.push_back(ParseFunctionCallExpression());
                    break;
                }

                report_and_consume_unexpected_token();
                break;
            }
            case TokenTypes::Keyword_Struct:
            {
                module->TopLevelNodes.push_back(ParseStructDeclarationStatement());
                break;
            }
            case TokenTypes::Semicolon:
            {
                advance_one_token();
                break;
            }
            default:
            {
                report_and_consume_unexpected_token();
                break;
            }
            }
        }

        return module;
    }; // namespace AST
};
} // namespace JSlang::AST
