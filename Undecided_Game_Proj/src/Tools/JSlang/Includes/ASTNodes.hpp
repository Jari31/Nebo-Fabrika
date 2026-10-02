#pragma once
#include "ArenaAllocator.hpp"
#include "Diagnostics.hpp"
#include "Lexer.hpp"
#include <cstdint>

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
    IfExpression,           // conditional expression; if(){} else {}
    SwitchExpression,       // switch () {}
    CaseExpression,         // case N ->
    ArrayAccessExpression,  // Something[]
    ExpectFromExpression,   // expect_from A  B

    ImplicitMemberAccessExpression,        // .Member
    ExplicitMemberAccessExpression,        // Object.Member
                                           //
    VariableDeclarationStatement,          // type my_var = 1;
    AliasStatement,                        // alias Something = SomethingElse
    DiscardAliasStatement,                 // discard alias Something
    FunctionDeclarationStatement,          // void func(){ ... }
    BlockStatement,                        // { ... }
    ReturnStatement,                       // return;
    ExpressionStatement,                   //
    ForStatement,                          // for () | | {}
    WhileStatement,                        // while () {}
    BreakStatement,                        // break;
    ContinueStatement,                     // continue;
    ImportStatement,                       // import Path;
    UnsafeStatement,                       // as unsafe
    StructDeclarationStatement,            // struct Identifier: = {}
    EmbeddedCodeblockDeclarationStatement, // begin X ... ||endX
    DefineMacroStatement,                  // define_macro A(Value: Type);

    Annotation, // @Annotation
};

struct ASTNode
{
    NodeTypes      NodeType;
    SourceLocation ObjectSourceLocation;
};

namespace AST
{
using Handle = ArenaAllocator::Handle;

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
    Handle From;
    Handle As;

    AliasStatement(SourceLocation ParameterSourceLocation)
    {
        NodeType                   = NodeTypes::AliasStatement;
        this->ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct DiscardAliasStatement : ASTNode
{
    Handle AliasName;

    DiscardAliasStatement(SourceLocation ParameterSourceLocation)
    {
        NodeType                   = NodeTypes::DiscardAliasStatement;
        this->ObjectSourceLocation = ParameterSourceLocation;
    }
};

/// the first node in the attributes list is always the type.
struct VariableDeclarationStatement : ASTNode
{
    bool   IsImmutable = false;
    Handle Attributes;

    Handle VariableName;
    Handle Initializer; // RHS; e.g., var/const VariableName: VariableTypeName = Initializer;

    VariableDeclarationStatement(SourceLocation ParameterSourceLocation)
    {
        NodeType                   = NodeTypes::VariableDeclarationStatement;
        this->ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct BinaryExpression : ASTNode
{
    TokenTypes OperandTokenType;
    Handle     LeftHandSide;
    Handle     RightHandSide;

    BinaryExpression(
        TokenTypes     ParameterTokenType,
        Handle         ParameterLeftHandSide,
        Handle         ParameterRightHandSide,
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
    Handle Arguments;

    FunctionCallExpression(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::FunctionCallExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct AnnotationFunctionExpression : ASTNode // @Identifier : { Decorations }
                                              // OR @Identifier() : { Decorations }
{
    Handle Identifier;
    Handle Arguments;

    Handle Decorations;

    AnnotationFunctionExpression(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::Annotation;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct UnaryExpression : ASTNode
{
    TokenTypes OperandType;
    Handle     Operand;

    UnaryExpression(
        SourceLocation ParameterSourceLocation,
        TokenTypes     ParameterOperand,
        Handle         ParameterIdentifier)
        : OperandType(ParameterOperand), Operand(ParameterIdentifier)
    {
        NodeType             = NodeTypes::UnaryExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    };
};

struct ExplicitMemberAccessExpression : ASTNode
{
    Handle Target;
    Handle TargetMember;

    ExplicitMemberAccessExpression(
        SourceLocation ParameterSourceLocation,
        Handle         ParameterTarget,
        Handle         ParameterTargetMember)
        : Target(ParameterTarget), TargetMember(ParameterTargetMember)
    {
        NodeType             = NodeTypes::ExplicitMemberAccessExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct ImplicitMemberAccessExpression : ASTNode
{
    Handle TargetMember;

    ImplicitMemberAccessExpression(
        SourceLocation ParameterSourceLocation,
        Handle         ParameterTargetMember)
        : TargetMember(ParameterTargetMember)
    {
        NodeType             = NodeTypes::ImplicitMemberAccessExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct BlockStatement : ASTNode
{
    Handle Statements;

    BlockStatement(SourceLocation ParameterSourceLocation, Handle ParameterStatements)
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
    Handle ReturnType;
    Handle Identifier;
    Handle Parameters;

    Handle Attributes;
    Handle FunctionBody;

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
    Handle Expression;

    ReturnStatement(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::ReturnStatement;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct ExpressionStatement : ASTNode
{
    Handle Expression;

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

    Handle Condition;
    Handle ThenBranch;
    Handle ElseBranch;

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
        Handle ForCondition; // if this is empty, then it is a default case
        Handle ThenExpression;

        SourceLocation ObjectCaseSourceLocation;
    };

    bool EvaluatedAtCompileTime = false;

    Handle Condition;
    Handle Cases;

    SwitchExpression(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::SwitchExpression;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct ForStatement : ASTNode
{
    Handle Condition;
    Handle Captures;
    Handle BlockStatement;

    ForStatement(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::ForStatement;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};

struct WhileStatement : ASTNode
{
    Handle Condition;
    Handle BlockStatement;

    WhileStatement(SourceLocation ParameterSourceLocation)
    {
        ObjectSourceLocation = ParameterSourceLocation;
        NodeType             = NodeTypes::WhileStatement;
    }
};

struct ImportStatement : ASTNode
{
    Handle ImportFrom; // import From/From From
    Handle ImportAs;   // as unsafe ; as something_else

    ImportStatement(SourceLocation ParameterSourceLocation)
    {
        ObjectSourceLocation = ParameterSourceLocation;
        NodeType             = NodeTypes::ImportStatement;
    }
};

struct StructDeclarationStatement : ASTNode
{
    Handle Identifier;
    Handle Attributes;

    Handle StructImplementation;

    StructDeclarationStatement(SourceLocation ParameterSourceLocation)
    {
        ObjectSourceLocation = ParameterSourceLocation;
        NodeType             = NodeTypes::StructDeclarationStatement;
    }
};

/// the source location contains the identifier of the array being accessed
struct ArrayAccessExpression : ASTNode
{
    Handle Expression;

    ArrayAccessExpression(SourceLocation ParameterSourceLocation)
    {
        ObjectSourceLocation = ParameterSourceLocation;
        NodeType             = NodeTypes::ArrayAccessExpression;
    }
};

struct ExpectFromExpression : ASTNode
{
    Handle ExpectedNode;
    Handle TargetNode;

    ExpectFromExpression(SourceLocation ParameterSourceLocation)
    {
        ObjectSourceLocation = ParameterSourceLocation;
        NodeType             = NodeTypes::ExpectFromExpression;
    }
};

struct DefineMacroStatement : ASTNode
{
    Handle Identifier;
    Handle Parameters;
    Handle Decorations;

    DefineMacroStatement(SourceLocation ParameterSourceLocation)
    {
        NodeType             = NodeTypes::DefineMacroStatement;
        ObjectSourceLocation = ParameterSourceLocation;
    }
};
} // namespace AST
} // namespace JSlang
