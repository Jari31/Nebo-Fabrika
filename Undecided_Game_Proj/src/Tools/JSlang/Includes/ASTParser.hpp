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

enum class BufferTypes : uint8_t
{
    None,
    PushConstant,
    SSBO,
    UniformBuffer
};

struct VariableDeclaration : ASTNode
{
    bool IsUniform;
    bool IsConstant;

    std::string_view VariableTypeName;
    std::string_view VariableName;
    ASTNode         *Initializer; // RHS; e.g., TypeName VariableName = Initializer;

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
    std::string_view Source;

    LiteralExpression(SourceLocation ParameterSourceLocation)
        : Source(ParameterSourceLocation.Source)
    {
        NodeType             = NodeTypes::LiteralExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct IdentifierExpression : ASTNode
{
    std::string_view Source;

    IdentifierExpression(SourceLocation ParameterSourceLocation)
        : Source(ParameterSourceLocation.Source)
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

    ASTNode *FunctionBody;

    FunctionDeclarationStatement(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::FunctionDeclarationStatement;
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

    bool match_with_next_token(TokenTypes TokenType)
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
        case TokenTypes::Plus:
        case TokenTypes::PlusPlus:
        case TokenTypes::Minus:
        case TokenTypes::MinusMinus:
        {
            return 10;
        }
        case TokenTypes::Star:
        case TokenTypes::Slash:
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

    template <TokenTypes ExpectedTerminator, ErrorCodes ErrorCode>
    std::span<ASTNode *> ParseArgumentativeExpressions(
        std::string ExpectedTerminatorErrorMessage,
        std::string ExpectedTerminatorMonologue)
    {
        advance_one_token();
        std::vector<ASTNode *> temporary_ast_node_pointer_vector;
        while (!check_token_type_of_current_token(ExpectedTerminator) ||
               !check_token_type_of_current_token(TokenTypes::EndOfFile))
        {
            temporary_ast_node_pointer_vector.push_back(ParseExpression());

            // ,)
            if (!match_with_next_token(TokenTypes::Comma))
            {
                break;
            }
        }

        expect_token_with_type(
            ExpectedTerminator,
            std::move(ExpectedTerminatorErrorMessage),
            std::move(ExpectedTerminatorMonologue),
            "",
            ErrorCode);

        std::span<ASTNode *> ast_node_pointer_slice =
            ObjectArenaAllocator.AllocateArray<ASTNode *>(temporary_ast_node_pointer_vector.size());
        std::ranges::copy(temporary_ast_node_pointer_vector, ast_node_pointer_slice.begin());

        return ast_node_pointer_slice;
    }

    std::span<ASTNode *> ParseFunctionArguments()
    {
        return ParseArgumentativeExpressions<
            TokenTypes::RightParenthesis,
            EXPECTED_RIGHT_PARENTHESIS>(
            "Expected ')' after '('.",
            "Lord... it's a wonder you got so far with your wits, mister. Close your damn '(' with "
            "a ')'.");
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

            advance_one_token();
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
     */
    ASTNode *ParseExpression(uint32_t MinimumPrecedence = 0)
    {
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
            case TokenTypes::LeftParenthesis:
            {
                left_hand_side = ParseFunctionCallExpression();
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

                if (left_hand_side != nullptr)
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

    ASTNode *ParseVariableDeclarationStatement()
    {
        SourceLocation start_location = CurrentToken.ObjectSourceLocation;

        bool is_variable_uniform  = match_with_next_token(TokenTypes::KeyWord_Uniform);
        bool is_variable_constant = match_with_next_token(TokenTypes::KeyWord_Constant);

        Token variable_type_name = expect_token_with_type(
            TokenTypes::Identifier,
            "Expected a type before variable declaration.",
            "Again, I ain't a damn magician nor book keeper, mister! Specify your damn type, like "
            "a 'float' or somethin'!");

        Token variable_name = expect_token_with_type(
            TokenTypes::Identifier,
            "Expected a variable name.",
            "Now, do I gotta explain why this ain't gonna work? There ain't no damned name after "
            "the type!");

        ASTNode *initializer = nullptr;

        if (match_with_next_token(TokenTypes::Equal))
        {
            initializer = ParseExpression();
        }

        expect_semicolon();

        auto *variable_declaration_node =
            ObjectArenaAllocator.Allocate<VariableDeclaration>(start_location);

        variable_declaration_node->VariableName = variable_name.ObjectSourceLocation.Source;
        variable_declaration_node->VariableTypeName =
            variable_type_name.ObjectSourceLocation.Source;
        variable_declaration_node->IsConstant  = is_variable_constant;
        variable_declaration_node->IsUniform   = is_variable_uniform;
        variable_declaration_node->Initializer = initializer;

        return variable_declaration_node;
    }

    // expected input: { decoration1, decor2, decor3 }
    std::span<ASTNode *> ParseDecorations()
    {
        return ParseArgumentativeExpressions<TokenTypes::RightParenthesis, EXPECTED_RIGHT_BRACKET>(
            "Expected '}' after '{'.",
            "Lord... it's a wonder you got so far with your wits, mister. Close your damn '{' with "
            "a '}'.");
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

    ASTNode *ParseFunctionDeclaration();

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
