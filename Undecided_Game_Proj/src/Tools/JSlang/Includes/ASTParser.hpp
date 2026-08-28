#pragma once
#include "ArenaAllocator.hpp"
#include "Diagnostics.hpp"
#include "ErrorCodes.hpp"
#include "Lexer.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace JSlang
{
enum class NodeTypes : uint8_t
{
    LiteralExpression,
    IdentifierExpression,
    BinaryExpression,
    CallExpression,
    VectorSwizzleExpression,
    LanguageEmbeddingExpression,

    VariableDeclarationStatement,
    AliasStatement,
    DiscardAliasStatement,
    FunctionDeclarationStatement,
    BlockStatement,

    Annotation,
};

struct ASTNode
{
    NodeTypes      NodeType;
    SourceLocation ObjectSourceLocation;
};

namespace AST
{

struct AliasStatement : public ASTNode
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

struct DiscardAliasStatement : public ASTNode
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

struct Annotation
{
};

struct VariableDeclaration : public ASTNode
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

struct BinaryExpression : public ASTNode
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

struct LanguageEmbeddingExpression : ASTNode
{
    std::string_view Language;

    LanguageEmbeddingExpression(
        std::string_view ParameterLanguage,
        SourceLocation   ParameterSourceLocation)
        : Language(ParameterLanguage)
    {
        NodeType             = NodeTypes::LanguageEmbeddingExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct Parser
{
    struct Module : public ASTNode
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
        CurrentToken    = PeekToken;
        PeekToken       = ObjectLexer.GetNextToken();

        return old_token;
    }

    [[nodiscard]] bool check_token_type_of_current_token(TokenTypes TokenType) const
    {
        return CurrentToken.TokenType == TokenType;
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

    Token
    expect_token_with_type(TokenTypes TokenType, std::string ErrorMessage, std::string Monologue)
    {
        if (check_token_type_of_current_token(TokenType))
        {
            return advance_one_token();
        }

        ObjectDiagnosticEngine.Report(
            Severity::Error,
            UNEXPECTED_TYPE,
            CurrentToken.ObjectSourceLocation,
            std::move(ErrorMessage),
            std::move(Monologue));
        return CurrentToken;
    }

    void expect_semicolon()
    {
        expect_token_with_type(
            TokenTypes::Semicolon,
            "Expected ';' after variable declaration.",
            "Programming language 101: USE YER DAMN SEMI COLONS!");
    }

    static uint32_t get_operator_precedence(TokenTypes TokenType)
    {
        switch (TokenType)
        {
        case TokenTypes::Plus:
        case TokenTypes::Minus:
        {
            return 10;
        }
        case TokenTypes::Star:
        case TokenTypes::Slash:
        {
            return 20;
        }
        case TokenTypes::Dot:
        {
            return 30;
        }
        default:
        {
            return 0;
        }
        }
    }

    ASTNode *ParsePrimary()
    {
        SourceLocation start_location = CurrentToken.ObjectSourceLocation;

        switch (CurrentToken.TokenType)
        {
        case TokenTypes::IntegerLiteral:
        case TokenTypes::FloatLiteral:
        {

            return ObjectArenaAllocator.Allocate<LiteralExpression>(start_location);
        }

        case TokenTypes::Identifier:
        {
            return ObjectArenaAllocator.Allocate<IdentifierExpression>(start_location);
        }
        case TokenTypes::LeftParenthesis:
        {
            ASTNode *expression = ParseExpression(0);
            expect_token_with_type(
                TokenTypes::RightParenthesis,
                "Expected ')' after parenthesized expression.",
                "Mister, you... You ain't the brightest tool in the shed, are ya? Close yer damn "
                "'(' with a ')'!");

            return expression;
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
        advance_one_token();
        return nullptr;
    };

    ASTNode *ParseExpression(uint32_t MinimumPrecedence = 0)
    {
        auto *left_hand_side = ParsePrimary();
        while (true)
        {
            uint32_t precedence = get_operator_precedence(CurrentToken.TokenType);
            if (precedence < MinimumPrecedence)
            {
                break;
            }

            Token operand_token = advance_one_token();

            auto *right_hand_side = ParseExpression(precedence + 1);

            left_hand_side = ObjectArenaAllocator.Allocate<BinaryExpression>(
                operand_token.TokenType,
                left_hand_side,
                right_hand_side,
                operand_token.ObjectSourceLocation);
        }
    };

    ASTNode *ParseVariableDeclaration()
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

    ASTNode *ParseLanguageEmbedding();
    ASTNode *ParseAnnotatedNode();
    ASTNode *ParseFunctionDeclaration();

    Module *ParseModule()
    {
        auto *module = ObjectArenaAllocator.Allocate<Module>();

        while (!check_token_type_of_current_token(TokenTypes::EndOfFile))
        {
            switch (CurrentToken.TokenType)
            {
            case TokenTypes::EmbeddedLanguageCodeblock:
            {
                module->TopLevelNodes.push_back(ParseLanguageEmbedding());
                break;
            }
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
                    "You sure don't look like you'd get very far on your wits.")
            }
            }
        }
    }
};

} // namespace AST
} // namespace JSlang
