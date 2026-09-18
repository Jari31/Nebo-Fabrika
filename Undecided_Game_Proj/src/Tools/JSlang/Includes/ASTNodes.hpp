#pragma once
#include "Diagnostics.hpp"
#include "Lexer.hpp"
#include <cstdint>
#include <span>
#include <string_view>

namespace JSlang
{
enum class NodeTypes : uint8_t
{
    LiteralExpression,              // 10
    IdentifierExpression,           // my_var
    BinaryExpression,               // 1 + 2
    FunctionCallExpression,         // func()
    UnaryExpression,                // ++var OR -var
    FlagExpression,                 // Flag
    IfExpression,                   // conditional expression; if(){} else {}
    SwitchExpression,               // switch () {}
    CaseExpression,                 // case N ->
                                    //
    ImplicitMemberAccessExpression, // .Member
    ExplicitMemberAccessExpression, // Object.Member
                                    //
    VariableDeclarationStatement,   // type my_var = 1;
    AliasStatement,                 // alias Something = SomethingElse
    DiscardAliasStatement,          // discard alias Something
    FunctionDeclarationStatement,   // void func(){ ... }
    BlockStatement,                 // { ... }
    ReturnStatement,                // return;
    ExpressionStatement,            //
    ForStatement,                   // for () | | {}
    WhileStatement,                 // while () {}
    BreakStatement,                 // break;
    ContinueStatement,              // continue;
    ImportStatement,                // import Path;
    UnsafeStatement,                // as unsafe
    StructDeclarationStatement,     // struct Identifier: = {}

    Annotation, // @Annotation
};

struct ASTNode
{
    NodeTypes      NodeType;
    SourceLocation ObjectSourceLocation;
};

namespace AST
{

struct GenericStatement : ASTNode
{
    GenericStatement(SourceLocation ParameterSourceLocation, NodeTypes ParameterNodeType)
    {
        ObjectSourceLocation = ParameterSourceLocation;
        NodeType             = ParameterNodeType;
    }
};

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

/// the first node in the attributes list is always the type.
struct VariableDeclarationStatement : ASTNode
{
    bool                 IsImmutable = false;
    std::span<ASTNode *> Attributes;

    std::string_view VariableName;
    ASTNode *Initializer; // RHS; e.g., var/const VariableName: VariableTypeName = Initializer;

    VariableDeclarationStatement(SourceLocation ParameterSourceLocation)
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
    std::span<ASTNode *> Arguments;

    FunctionCallExpression(SourceLocation ParameterSourceLocation)
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
    // maybe we should make this into a SourceLocation instead of a string view for more accurate
    // errors
    std::string_view     ReturnType;
    std::string_view     Identifier;
    std::span<ASTNode *> Parameters;

    std::span<ASTNode *> Attributes;
    ASTNode             *FunctionBody;

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

// return statement
struct ReturnStatement : ASTNode
{
    ASTNode *Expression;

    ReturnStatement(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::ReturnStatement;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct ExpressionStatement : ASTNode
{
    ASTNode *Expression;

    ExpressionStatement(SourceLocation ParameterSourceLocation, ASTNode *ParameterExpression)
        : Expression(ParameterExpression)
    {
        NodeType             = NodeTypes::ExpressionStatement;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct IfExpression : ASTNode
{
    bool EvaluateAtCompileTime = true;

    ASTNode *Condition;
    ASTNode *ThenBranch;
    ASTNode *ElseBranch;

    IfExpression(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::IfExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct SwitchExpression : ASTNode
{
    struct Case
    {
        std::span<ASTNode *> ForCondition; // if this is empty, then it is a default case
        ASTNode             *ThenExpression;

        SourceLocation ObjectCaseSourceLocation;
    };

    bool EvaluatedAtCompileTime = false;

    ASTNode          *Condition;
    std::span<Case *> Cases;

    SwitchExpression(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::SwitchExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct ForStatement : ASTNode
{
    ASTNode             *Condition;
    std::span<ASTNode *> Captures;
    ASTNode             *BlockStatement;

    ForStatement(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::ForStatement;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct WhileStatement : ASTNode
{
    ASTNode *Condition;
    ASTNode *BlockStatement;

    WhileStatement(SourceLocation ParameterSourceLocation)
    {
        ObjectSourceLocation = ParameterSourceLocation;
        NodeType             = NodeTypes::WhileStatement;
    }
};

struct ImportStatement : ASTNode
{
    std::string_view ImportFrom; // import From/From From
    ASTNode         *ImportAs;   // as unsafe ; as something_else

    ImportStatement(SourceLocation ParameterSourceLocation)
    {
        ObjectSourceLocation = ParameterSourceLocation;
        NodeType             = NodeTypes::ImportStatement;
    }
};

struct StructDeclarationStatement : ASTNode
{
    std::string_view     Identifier;
    std::span<ASTNode *> Attributes;

    std::span<ASTNode *> StructImplementation;

    StructDeclarationStatement(SourceLocation ParameterSourceLocation)
    {
        ObjectSourceLocation = ParameterSourceLocation;
        NodeType             = NodeTypes::StructDeclarationStatement;
    }
};
} // namespace AST
} // namespace JSlang
