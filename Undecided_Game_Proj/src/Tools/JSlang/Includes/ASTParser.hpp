#pragma once
#include "ArenaAllocator.hpp"
#include "Diagnostics.hpp"
#include "ErrorCodes.hpp"
#include "Lexer.hpp"
#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace JSlang
{
enum class NodeTypes : uint8_t
{
    LiteralExpression,      // 10
    IdentifierExpression,   // my_var
    BinaryExpression,       // 1 + 2
    FunctionCallExpression, // func()
    UnaryExpression,        // ++var OR -var
    FlagExpression,         // Flag

    ImplicitMemberAccessExpression, // .Member
    ExplicitMemberAccessExpression, // Object.Member

    VariableDeclarationStatement, // type my_var = 1;
    AliasStatement,               // alias Something = SomethingElse
    DiscardAliasStatement,        // discard alias Something
    FunctionDeclarationStatement, // void func(){ ... }
    BlockStatement,               // { ... }

    Annotation, // @Annotation
};

struct ASTNode
{
    NodeTypes      NodeType;
    SourceLocation ObjectSourceLocation;
};

namespace AST
{

enum class DeclarationFlags : uint32_t // NOLINT
{
    None     = 0,      // 00000000 00000000
    Inline   = 1 << 0, // 00000000 00000001
    Uniform  = 1 << 1, // 00000000 00000010
    Constant = 1 << 2, // 00000000 00000100
    Static   = 1 << 3, // 00000000 00001000
    Export   = 1 << 4, // 00000000 00010000
};

DeclarationFlags operator|(DeclarationFlags FlagA, DeclarationFlags FlagB)
{
    using Type = std::underlying_type_t<DeclarationFlags>;
    return static_cast<DeclarationFlags>(static_cast<Type>(FlagA) | static_cast<Type>(FlagB));
}

struct AliasStatement : ASTNode
{
    std::string_view AliasName;
    std::string_view TargetName;

    AliasStatement(std::string_view AliasName, std::string_view TargetName, SourceLocation Location)
        : AliasName(AliasName), TargetName(TargetName)
    {
        NodeType                   = NodeTypes::AliasStatement;
        this->ObjectSourceLocation = Location;
    }
};

struct DiscardAliasStatement : ASTNode
{
    std::string_view AliasName;

    DiscardAliasStatement(std::string_view AliasName, SourceLocation SourceLocation)
        : AliasName(AliasName)
    {
        NodeType                   = NodeTypes::DiscardAliasStatement;
        this->ObjectSourceLocation = SourceLocation;
    }
};

struct VariableDeclaration : ASTNode
{
    std::span<ASTNode *> Attributes;

    std::string_view VariableTypeName;
    std::string_view VariableName;
    ASTNode *Initializer; // RHS; e.g., var/const VariableName: VariableTypeName = Initializer;

    VariableDeclaration(SourceLocation ParameterSourceLocation)
    {
        NodeType                   = NodeTypes::VariableDeclarationStatement;
        this->ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct BinaryExpression : ASTNode
{
    TokenTypes OperandTokenType;
    ASTNode   *LeftHandSide;
    ASTNode   *RightHandSide;

    BinaryExpression(
        TokenTypes     ParameterTokenType,
        ASTNode       *ParameterLeftHandSide,
        ASTNode       *ParameterRightHandSide,
        SourceLocation ParameterSourceLocation)
        : OperandTokenType(ParameterTokenType), LeftHandSide(ParameterLeftHandSide),
          RightHandSide(ParameterRightHandSide)
    {
        NodeType             = NodeTypes::BinaryExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct LiteralExpression : ASTNode
{
    LiteralExpression(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::LiteralExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct IdentifierExpression : ASTNode
{
    IdentifierExpression(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::IdentifierExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct FunctionCallExpression : ASTNode
{
    std::string_view     Identifier;
    std::span<ASTNode *> Arguments;

    FunctionCallExpression(
        std::span<ASTNode *> ParameterArguments,
        SourceLocation       ParameterSourceLocation,
        std::string_view     ParameterIdentifier)
        : Identifier(ParameterIdentifier), Arguments(ParameterArguments)
    {
        NodeType             = NodeTypes::FunctionCallExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct AnnotationFunctionExpression : ASTNode // @Identifier : { Decorations }
                                              // OR @Identifier() : { Decorations }
{
    std::string_view     Identifier;
    std::span<ASTNode *> Arguments;

    std::span<ASTNode *> Decorations;

    [[nodiscard]] bool IsFunction() const { return !Arguments.empty(); }
    [[nodiscard]] bool ContainsDecorations() const { return !Decorations.empty(); }

    AnnotationFunctionExpression(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::Annotation;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct UnaryExpression : ASTNode
{
    TokenTypes OperandType;
    ASTNode   *Operand;

    UnaryExpression(
        SourceLocation ParameterSourceLocation,
        TokenTypes     ParameterOperand,
        ASTNode       *ParameterIdentifier)
        : OperandType(ParameterOperand), Operand(ParameterIdentifier)
    {
        NodeType             = NodeTypes::UnaryExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    };
};

struct ExplicitMemberAccessExpression : ASTNode
{
    ASTNode         *Target;
    std::string_view TargetMember;

    ExplicitMemberAccessExpression(
        SourceLocation   ParameterSourceLocation,
        ASTNode         *ParameterTarget,
        std::string_view ParameterTargetMember)
        : Target(ParameterTarget), TargetMember(ParameterTargetMember)
    {
        NodeType             = NodeTypes::ExplicitMemberAccessExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct ImplicitMemberAccessExpression : ASTNode
{
    std::string_view TargetMember;

    ImplicitMemberAccessExpression(
        SourceLocation   ParameterSourceLocation,
        std::string_view ParameterTargetMember)
        : TargetMember(ParameterTargetMember)
    {
        NodeType             = NodeTypes::ImplicitMemberAccessExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct BlockStatement : ASTNode
{
    std::span<ASTNode *> Statements;
    BlockStatement(SourceLocation ParameterSourceLocation, std::span<ASTNode *> ParameterStatements)
        : Statements(ParameterStatements)
    {
        NodeType             = NodeTypes::BlockStatement;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct FunctionDeclarationStatement : ASTNode
{
    struct Parameter
    {
        std::string_view Type;
        std::string_view Identifier;

        SourceLocation ObjectSourceLocation;
    };

    std::string_view     ReturnType;
    std::string_view     Identifier;
    std::span<Parameter> Parameters;
    uint8_t              FunctionFlags;

    ASTNode *FunctionBody;

    FunctionDeclarationStatement(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::FunctionDeclarationStatement;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct FlagExpression : ASTNode
{
    FlagExpression(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::FlagExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct Parser
{
    struct Module : ASTNode
    {
        std::vector<ASTNode *> TopLevelNodes;
    };

    Lexer            &ObjectLexer;
    ArenaAllocator   &ObjectArenaAllocator;
    DiagnosticEngine &ObjectDiagnosticEngine;

    Token CurrentToken;
    Token PeekToken;

    Parser(
        Lexer            &ParameterLexer,
        ArenaAllocator   &ParameterArenaAllocator,
        DiagnosticEngine &ParameterDiagnosticEngine)
        : ObjectLexer(ParameterLexer), ObjectArenaAllocator(ParameterArenaAllocator),
          ObjectDiagnosticEngine(ParameterDiagnosticEngine)
    {
        advance_one_token();
        advance_one_token();
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
        return advance_one_token();
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
        case TokenTypes::LeftBracket:
        {
            return 30;
        }
        default:
        {
            return 0;
        }
        }
    }

    // skipping all the fancy names, it's just parsing blocks like { stuff1, stuff2, stuff3 } or (
    // stuff1, stuff2, stuff3 ) and such
    template <ErrorCodes ErrorCode>
    std::span<ASTNode *> ParseArgumentativeExpressionUntilTerminator(
        std::string ExpectedTerminatorErrorMessage,
        std::string ExpectedTerminatorMonologue,
        auto      &&CallbackIsCurrentTokenATerminator)
    {
        // an expression might be like var value : { arg, arg2 = {arg3, arg4} } = val;

        std::vector<ASTNode *> temporary_ast_node_pointer_vector;
        while (true)
        {
            if (CallbackIsCurrentTokenATerminator())
            {
                break;
            }

            auto *ast_node = ParseExpression();
            if (ast_node != nullptr)
            {
                temporary_ast_node_pointer_vector.push_back(ast_node);
            }

            if (!match_with_current_token(TokenTypes::Comma))
            {
                break;
            }
        }

        if (!CallbackIsCurrentTokenATerminator())
        {
            ObjectDiagnosticEngine.Report(
                Severity::Error,
                ErrorCode,
                CurrentToken.ObjectSourceLocation,
                std::move(ExpectedTerminatorErrorMessage),
                std::move(ExpectedTerminatorMonologue));
        }

        std::span<ASTNode *> ast_node_pointer_slice =
            ObjectArenaAllocator.AllocateArray<ASTNode *>(temporary_ast_node_pointer_vector.size());
        std::ranges::copy(temporary_ast_node_pointer_vector, ast_node_pointer_slice.begin());

        return ast_node_pointer_slice;
    }

    std::span<ASTNode *> ParseFunctionArguments()
    {
        // (arg1, arg2, arg3)

        advance_one_token(); // consume '('

        return ParseArgumentativeExpressionUntilTerminator<EXPECTED_RIGHT_PARENTHESIS>(
            "Expected ')' after '('.",
            "Lord... it's a wonder you got so far with your wits, mister. Close your damn '(' with "
            "a ')'.",
            [this]() { return check_token_type_of_current_token(TokenTypes::LeftParenthesis); });
    }

    ASTNode *ParseFunctionCallExpression()
    {
        auto start_location = CurrentToken.ObjectSourceLocation;
        auto identifier     = expect_token_with_type(
            TokenTypes::Identifier,
            "Expected identifier before parenthesis.",
            "Lord, please save me from this ignorance. How on earth do you think I'm supposed to "
            "track what the damn function even is if you don't take your damn time to write out "
            "the identifier?",
            "",
            EXPECTED_IDENTIFIER);

        std::span<ASTNode *> function_arguments;

        function_arguments = ParseFunctionArguments();

        return ObjectArenaAllocator.Allocate<FunctionCallExpression>(
            function_arguments, start_location, identifier.ObjectSourceLocation.Source);
    };

    ASTNode *ParseIfElseStatements();

    ASTNode *ParsePrimary()
    {
        SourceLocation start_location = CurrentToken.ObjectSourceLocation;

        switch (CurrentToken.TokenType)
        {
        case TokenTypes::IntegerLiteral:
        case TokenTypes::FloatLiteral:
        {
            advance_one_token();
            return ObjectArenaAllocator.Allocate<LiteralExpression>(start_location);
        }

        case TokenTypes::Identifier:
        {

            advance_one_token();
            return ObjectArenaAllocator.Allocate<IdentifierExpression>(start_location);
        }
        case TokenTypes::LeftParenthesis:
        {
            ASTNode *expression = ParseExpression(0);
            expect_token_with_type(
                TokenTypes::RightParenthesis,
                "Expected ')' after parenthesized expression.",
                "Mister, you... You ain't the brightest tool in the shed, are ya? Close yer damn "
                "'(' with a ')'!",
                "Close '(' with ')'.",
                EXPECTED_RIGHT_PARENTHESIS);

            return expression;
        }
        case TokenTypes::LeftBracket:
        {
            ASTNode *expression = ParseExpression(0);
            expect_token_with_type(
                TokenTypes::RightBracket,
                "Expected '}' after bracketed expression.",
                "Mister, you... You ain't the brightest tool in the shed, are ya? Close yer damn "
                "'{' with a '}'!",
                "Close '{' with '}'.",
                EXPECTED_RIGHT_BRACKET);

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

    /*  @brief rough example:
     *  Given: 1 + 2 * 3
     *  lhs = 1; consume 1
     *
     *  operand = '+'; consume '+' // cursor position = 2 * 3
     *
     *  rhs =  --- recurse(1's precedence + 1)
     *      lhs = 2 : 2's precedence = 0; consume 2 // cursor position * 3
     *      operand = '*' : precedence = 30 // cursor position 3
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
     *  a + b * f(c, d)
     *  lhs = 1
     *  operand = + : 10
     *  rhs = (11) {
     *      lhs = 2
     *      operand = * : 20
     *      rhs = (21) {
     *          return parse function args;
     * }
     * }
     */
    ASTNode *ParseExpression(uint32_t MinimumPrecedence = 0)
    {
        /*
         *  lhs = inline; precedence 0
         *  operand = comma; precedence 0
         *  create singular node and return {inline}
         */

        auto *left_hand_side = ParsePrimary();
        while (true)
        {
            uint32_t precedence = get_operator_precedence(CurrentToken.TokenType);
            if (precedence < MinimumPrecedence ||
                check_token_type_of_current_token(TokenTypes::EndOfFile))
            {
                break;
            }

            Token operand_token = advance_one_token();

            switch (operand_token.TokenType)
            {
                // var variable = if(condition1 > condition2) value1 else if (condition2 >=
                // condition1) value2 else value3
            case TokenTypes::KeyWord_If:
            {
                left_hand_side = ParseIfElseStatements();
                break;
            }
            case TokenTypes::LeftParenthesis:
            {
                if (left_hand_side != nullptr &&
                    left_hand_side->NodeType == NodeTypes::IdentifierExpression)
                {
                    left_hand_side = ParseFunctionCallExpression();
                }
                break;
            }
            case TokenTypes::Dot:
            {
                auto target_member = expect_token_with_type(
                    TokenTypes::Identifier,
                    "Expected identifier after access operator ('.').",
                    "I've seen things, mister. But never, even from Micah, have I seen such "
                    "idiocy. PUT A DAMN WORD OR SOMETHIN' AFTER YOUR '.'! Expect me to read your "
                    "damn mind "
                    "otherwise?!",
                    "",
                    EXPECTED_IDENTIFIER);

                if (left_hand_side != nullptr &&
                    left_hand_side->NodeType == NodeTypes::IdentifierExpression)
                {
                    left_hand_side = ObjectArenaAllocator.Allocate<ExplicitMemberAccessExpression>(
                        operand_token.ObjectSourceLocation,
                        left_hand_side,
                        target_member.ObjectSourceLocation.Source);

                    break;
                }

                left_hand_side = ObjectArenaAllocator.Allocate<ImplicitMemberAccessExpression>(
                    operand_token.ObjectSourceLocation, target_member.ObjectSourceLocation.Source);
            }
            case TokenTypes::Comma:
            {

                if (left_hand_side != nullptr &&
                    left_hand_side->NodeType == NodeTypes::IdentifierExpression)
                {
                    left_hand_side = ObjectArenaAllocator.Allocate<FlagExpression>();
                }

                break;
            }
            default:
            {
                auto *right_hand_side = ParseExpression(precedence + 1);

                left_hand_side = ObjectArenaAllocator.Allocate<BinaryExpression>(
                    operand_token.TokenType,
                    left_hand_side,
                    right_hand_side,
                    operand_token.ObjectSourceLocation);
            }
            }
        }

        return left_hand_side;
    };

    std::span<ASTNode *> ParseVariableAttributes()
    {
        // var identifier: attributes = value;
        // struct identifier: attributes {};

        switch (CurrentToken.TokenType)
        {
        case TokenTypes::LeftBracket:
        {
            return ParseArgumentativeExpressionUntilTerminator<EXPECTED_RIGHT_PARENTHESIS>(
                "Expected 'End Of File' or ';' after ':'.",
                "Think I've seen bricks with more wit than you, mister. Place a damn 'End Of "
                "File' or ';' after ':'!",
                [this]()
                {
                    switch (CurrentToken.TokenType)
                    {
                    case TokenTypes::EndOfFile:
                    case TokenTypes::RightBracket:
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
        case TokenTypes::Equal:
        {
            return {};
        }
        case TokenTypes::Identifier:
        {
        }
        }
    }

    template <bool IsImmutableVariable = false> ASTNode *ParseVariableDeclarationStatement()
    {
        SourceLocation start_location = CurrentToken.ObjectSourceLocation;

        auto *variable_declaration_node =
            ObjectArenaAllocator.Allocate<VariableDeclaration>(start_location);

        if constexpr (IsImmutableVariable)
        {
            variable_declaration_node->Flags = DeclarationFlags::Constant;
        }

        ASTNode *initializer = nullptr;

        bool defines_variable_type = 0;
        if (match_with_current_token(TokenTypes::Colon))
        {
            advance_one_token();
            if (match_with_current_token(TokenTypes::Identifier) ||
                match_with_current_token(TokenTypes::LeftBracket))
            {
                variable_declaration_node->Attributes = ParseVariableAttributes();
            }
        }

        if (match_with_current_token(TokenTypes::Equal))
        {
            initializer = ParseExpression();
        }

        expect_semicolon();

        variable_declaration_node->VariableName = variable_name.ObjectSourceLocation.Source;
        variable_declaration_node->VariableTypeName =
            variable_type_name.ObjectSourceLocation.Source;
        variable_declaration_node->Initializer = initializer;

        return variable_declaration_node;
    }

    // expected input: { decoration1, decor2, decor3 }
    std::span<ASTNode *> ParseDecorations()
    {
        if (check_token_type_of_current_token(TokenTypes::LeftBracket))
        {
            return ParseArgumentativeExpressionUntilTerminator<
                TokenTypes::RightParenthesis,
                EXPECTED_RIGHT_BRACKET>(
                "Expected '}' after '{'.",
                "Lord... it's a wonder you got so far with your wits, mister. Close your damn '{' "
                "with "
                "a '}'.");
        }
        return
    }

    ASTNode *ParseAnnotatedNode()
    {
        auto start_location = CurrentToken.ObjectSourceLocation;

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

        if (check_token_type_of_current_token(TokenTypes::LeftParenthesis))
        {
            annotated_node->Arguments = ParseFunctionArguments();
        }

        if (check_token_type_of_current_token(TokenTypes::Colon))
        {
            advance_one_token(); // consume ':'
            annotated_node->Decorations = ParseDecorations();
        }

        return annotated_node;
    }

    ASTNode *ParseFunctionDeclaration() {

    };

    Module *ParseModule()
    {
        auto *module = ObjectArenaAllocator.Allocate<Module>();

        while (!check_token_type_of_current_token(TokenTypes::EndOfFile))
        {
            switch (CurrentToken.TokenType)
            {
            case TokenTypes::AtSymbol:
            {
                module->TopLevelNodes.push_back(ParseAnnotatedNode());
                break;
            }
            case TokenTypes::KeyWord_Export:
            case TokenTypes::KeyWord_Inline:
            case TokenTypes::KeyWord_Void:
            {
                module->TopLevelNodes.push_back(ParseFunctionDeclaration());
                break;
            }
            default:
            {
                ObjectDiagnosticEngine.Report(
                    Severity::Error,
                    UNRECOGNIZED_TOP_LEVEL_NODE,
                    CurrentToken.ObjectSourceLocation,
                    "Unrecognized top level node.",
                    "You sure don't look like you'd get very far on your wits.");
            }
            }
        }

        return module;
    }
};

} // namespace AST
} // namespace JSlang
