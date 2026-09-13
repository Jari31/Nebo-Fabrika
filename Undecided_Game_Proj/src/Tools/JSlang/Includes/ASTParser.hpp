#pragma once
#include "ArenaAllocator.hpp"
#include "Diagnostics.hpp"
#include "ErrorCodes.hpp"
#include "Lexer.hpp"
#include <algorithm>
#include <cstdint>
#include <print>
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
    FlagExpression,         // Flag
    IfExpression,           // conditional expression; if(){} else {}
    SwitchExpression,
    CaseExpression,

    ImplicitMemberAccessExpression, // .Member
    ExplicitMemberAccessExpression, // Object.Member

    VariableDeclarationStatement, // type my_var = 1;
    AliasStatement,               // alias Something = SomethingElse
    DiscardAliasStatement,        // discard alias Something
    FunctionDeclarationStatement, // void func(){ ... }
    BlockStatement,               // { ... }
    ReturnStatement,
    ExpressionStatement,
    ForStatement,
    WhileStatement,
    BreakStatement,
    ContinueStatement,

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
struct VariableDeclaration : ASTNode
{
    bool                 IsImmutable = false;
    std::span<ASTNode *> Attributes;

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
    /// The source location contains the name of the type. Likewise, the first node within the
    /// attribute array contains the type.
    struct Parameter
    {
        SourceLocation       ObjectSourceLocation;
        std::span<ASTNode *> Attributes;
    };

    // maybe we should make this into a SourceLocation instead of a string view for more accurate
    // errors
    std::string_view       ReturnType;
    std::string_view       Identifier;
    std::span<Parameter *> Parameters;

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
        std::span<ASTNode *> IfCondition; // if this is empty, then it is a default case
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

    Parser(Lexer &ParameterLexer, ArenaAllocator &ParameterArenaAllocator)
        : ObjectLexer(ParameterLexer), ObjectArenaAllocator(ParameterArenaAllocator),
          ObjectDiagnosticEngine(ParameterLexer.ObjectDiagnosticEngine)
    {
        advance_one_token();
        advance_one_token();
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

    // skipping all the fancy names, it's just parsing blocks like { stuff1, stuff2, stuff3 } or (
    // stuff1, stuff2, stuff3 ) and such
    template <ErrorCodes ErrorCode, bool ConsumeInitializer = false, bool ConsumeTerminator = false>
    std::span<ASTNode *> ParseArgumentativeExpressionUntilTerminator(
        std::string ExpectedTerminatorErrorMessage,
        std::string ExpectedTerminatorMonologue,
        auto      &&CallbackIsCurrentTokenATerminator)
    {
        // an expression might be like var value : { arg, arg2 = {arg3, .arg4 = 41} } = val;
        if constexpr (ConsumeInitializer)
        {
            advance_one_token();
        }

        std::vector<ASTNode *> temporary_ast_node_pointer_vector;
        while (!CallbackIsCurrentTokenATerminator() &&
               !check_token_type_of_current_token(TokenTypes::EndOfFile))
        {
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
        else if constexpr (ConsumeTerminator)
        {
            if (!check_token_type_of_current_token(TokenTypes::EndOfFile))
            {
                advance_one_token();
            }
        }

        std::span<ASTNode *> ast_node_pointer_slice =
            ObjectArenaAllocator.AllocateArray<ASTNode *>(temporary_ast_node_pointer_vector.size());
        std::ranges::copy(temporary_ast_node_pointer_vector, ast_node_pointer_slice.begin());

        return ast_node_pointer_slice;
    }

    std::span<ASTNode *> ParseFunctionArguments()
    {
        // (arg1, arg2, arg3)
        return ParseArgumentativeExpressionUntilTerminator<EXPECTED_RIGHT_PARENTHESIS, true>(
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
                "Mister, you... You ain't the brightest tool in the shed, are ya? Close your damn "
                "'(' with a ')'!",
                "Close '(' with ')'.",
                EXPECTED_RIGHT_PARENTHESIS);

            return expression;
        }
        case TokenTypes::LeftBrace:
        {
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
                break;
            }
            }

            Token operator_token = advance_one_token();

            switch (operator_token.TokenType)
            {
            case TokenTypes::LeftParenthesis:
            {
                if (left_hand_side != nullptr &&
                    left_hand_side->NodeType == NodeTypes::IdentifierExpression)
                {
                    std::span<ASTNode *> function_arguments;

                    function_arguments = ParseFunctionArguments();

                    left_hand_side = ObjectArenaAllocator.Allocate<FunctionCallExpression>(
                        function_arguments,
                        operator_token.ObjectSourceLocation,
                        left_hand_side->ObjectSourceLocation.Source);
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
                        operator_token.ObjectSourceLocation,
                        left_hand_side,
                        target_member.ObjectSourceLocation.Source);

                    break;
                }

                left_hand_side = ObjectArenaAllocator.Allocate<ImplicitMemberAccessExpression>(
                    operator_token.ObjectSourceLocation, target_member.ObjectSourceLocation.Source);
                break;
            }
            default:
            {
                auto *right_hand_side = ParseExpression(precedence + 1);

                left_hand_side = ObjectArenaAllocator.Allocate<BinaryExpression>(
                    operator_token.TokenType,
                    left_hand_side,
                    right_hand_side,
                    operator_token.ObjectSourceLocation);

                std::print("Current token: {}\n", CurrentToken.ObjectSourceLocation.Source);
            }
            }
        }

        return left_hand_side;
    };

    std::span<ASTNode *> ParseVariableAttributes()
    {
        // var identifier: attributes = value;
        // struct identifier: attributes {};
        if (check_token_type_of_current_token(TokenTypes::LeftBrace))
        {
            return ParseArgumentativeExpressionUntilTerminator<
                EXPECTED_RIGHT_BRACE_OR_SEMICOLON,
                true,
                true>(
                "Expected '}' after ':'.",
                "Think I've seen bricks with more wit than you, mister. Place a damn '}' after "
                "':'!",
                [this]()
                {
                    switch (CurrentToken.TokenType)
                    {
                    case TokenTypes::Semicolon:
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

        auto parsed_variable_attributes = ParseArgumentativeExpressionUntilTerminator<
            EXPECTED_EQUAL_SIGN_OR_SEMICOLON>(
            "Expected '=' or ';' after attribute expression.",
            "Now, how'd you reckon I'm supposed to figure out where your damned code "
            "ends? Give me a damned semicolon or an equal sign, or take your ugly mug somewhere "
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
            });

        if (check_token_type_of_current_token(TokenTypes::Semicolon))
        {
            advance_one_token();
        }

        return parsed_variable_attributes;
    }

    template <bool IsImmutableVariable = false> ASTNode *ParseVariableDeclarationStatement()
    {
        SourceLocation start_location = CurrentToken.ObjectSourceLocation;
        advance_one_token(); // consume 'var/const'

        auto *variable_declaration_node =
            ObjectArenaAllocator.Allocate<VariableDeclaration>(start_location);

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
            advance_one_token();
            variable_declaration_node->Attributes = ParseVariableAttributes();
        }

        if (match_with_current_token(TokenTypes::Equal))
        {
            initializer = ParseExpression();
        }

        expect_semicolon();

        if (initializer != nullptr)
        {
            variable_declaration_node->Initializer = initializer;
        }

        return variable_declaration_node;
    }

    // expected input: { decoration1, decor2, decor3 }
    std::span<ASTNode *> ParseAttributes()
    {
        if (check_token_type_of_current_token(TokenTypes::LeftBrace))
        {
            return ParseArgumentativeExpressionUntilTerminator<EXPECTED_RIGHT_BRACE, true, true>(
                "Expected '}' after '{'.",
                "Lord... it's a wonder you got so far with your wits, mister. Close your damn '{' "
                "with "
                "a '}'.",
                [this]() { return check_token_type_of_current_token(TokenTypes::RightBrace); });
        }

        return ParseArgumentativeExpressionUntilTerminator<EXPECTED_SEMICOLON, false, true>(
            "Expected terminator ';' after annotation (macro) declaration.",
            "How do you think I'm supposed to know when your damn code ends? You thinkin' I'm a "
            "magician, mister? Put a damn semicolon (';') after your statement.",
            [this]() { return check_token_type_of_current_token(TokenTypes::Semicolon); });
    }

    ASTNode *ParseAnnotatedNode()
    {
        auto start_location = CurrentToken.ObjectSourceLocation;
        advance_one_token(); // consume '@'

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

        // std::print("Current token: {}\n", CurrentToken.ObjectSourceLocation.Source);
        if (check_token_type_of_current_token(TokenTypes::Colon))
        {
            advance_one_token(); // consume ':'
            annotated_node->Decorations = ParseAttributes();
        }

        return annotated_node;
    }

    template <bool ConsumeInitializer = true>
    std::span<FunctionDeclarationStatement::Parameter *> ParseFunctionDeclarationParameters()
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

        using Parameter = FunctionDeclarationStatement::Parameter;

        std::vector<Parameter *> temporary_parameter_pointer_vector;
        while (!check_token_type_of_current_token(TokenTypes::RightParenthesis))
        {
            auto *parameter = ObjectArenaAllocator.Allocate<Parameter>();
            parameter->ObjectSourceLocation =
                expect_token_with_type(
                    TokenTypes::Identifier,
                    "Expected identifier after parameter initialization.",
                    "For God's sake, place a damned identifier instead of whatever the hell you've "
                    "got there.")
                    .ObjectSourceLocation;

            if (check_token_type_of_current_token(TokenTypes::Semicolon))
            {
                parameter->Attributes =
                    ParseArgumentativeExpressionUntilTerminator<EXPECTED_SEMICOLON>(
                        "Expected delimiter (semicolon, ';') after decoration.",
                        "",
                        [this]()
                        { return check_token_type_of_current_token(TokenTypes::Semicolon); });
            }

            temporary_parameter_pointer_vector.push_back(parameter);
        }

        advance_one_token(); // consume ')'

        std::span<Parameter *> parameter_pointer_slice =
            ObjectArenaAllocator.AllocateArray<Parameter *>(
                temporary_parameter_pointer_vector.size());
        std::ranges::copy(temporary_parameter_pointer_vector, parameter_pointer_slice.begin());

        return parameter_pointer_slice;
    }

    /// doesn't consume trailing semicolons
    /// expected input: { attributes } OR attributes
    std::span<ASTNode *> ParseStructOrFunctionAttributes()
    {
        if (check_token_type_of_current_token(TokenTypes::LeftBrace))
        {
            return ParseArgumentativeExpressionUntilTerminator<EXPECTED_RIGHT_BRACE, true, true>(
                "Expected terminator '}' after '{'.",
                "",
                [this]() { return check_token_type_of_current_token(TokenTypes::RightBrace); });
        }

        auto parsed_function_attributes =
            ParseArgumentativeExpressionUntilTerminator<EXPECTED_LEFT_BRACE>(
                "Expected terminator '{' after ':'.",
                "",
                [this]()
                {
                    switch (CurrentToken.TokenType)
                    {
                    case TokenTypes::Semicolon:
                    case TokenTypes::LeftBrace:
                    {
                        return true;
                    }
                    default:
                    {
                        return false;
                    }
                    }
                });

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

    ASTNode *ParseBlockStatement()
    {
        auto start_location = CurrentToken.ObjectSourceLocation;
        advance_one_token(); // consume '{'

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

        auto statement_pointer_slice = ObjectArenaAllocator.AllocateArray<ASTNode *>(
            temporary_statement_pointer_vector.size());
        std::ranges::copy(temporary_statement_pointer_vector, statement_pointer_slice.begin());

        return ObjectArenaAllocator.Allocate<BlockStatement>(
            start_location, statement_pointer_slice);
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

        auto start_location = CurrentToken.ObjectSourceLocation;
        advance_one_token(); // consume "fn"

        auto *function_declaration_statement =
            ObjectArenaAllocator.Allocate<FunctionDeclarationStatement>(start_location);

        function_declaration_statement->Identifier =
            expect_token_with_type(
                TokenTypes::Identifier,
                "Expected identifier after function declaration statement.",
                "Mister, I'm getting tired of this. Just place a damned word after your function "
                "declaration statement or somethin'.")
                .ObjectSourceLocation.Source;

        function_declaration_statement->Parameters = ParseFunctionDeclarationParameters<true>();

        if (check_token_type_of_current_token(TokenTypes::RightArrow))
        {
            advance_one_token(); // consume '->'

            function_declaration_statement->ReturnType =
                expect_token_with_type(
                    TokenTypes::Identifier,
                    "Expected identifier after return type declarator initializer (->).",
                    "")
                    .ObjectSourceLocation.Source;
        }

        if (check_token_type_of_current_token(TokenTypes::Colon))
        {
            function_declaration_statement->Attributes = ParseStructOrFunctionAttributes();
        }

        if (check_token_type_of_current_token(TokenTypes::Semicolon)) // forward decl probably
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
            "Expected left brace ('{') or semicolon (';') after function declaration.",
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

        auto start_location = CurrentToken.ObjectSourceLocation;
        advance_one_token(); // consume 'switch'

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
                    switch_case_expression->IfCondition = ParseCaseExpression();
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

        auto cases_pointer_slice = ObjectArenaAllocator.AllocateArray<SwitchExpression::Case *>(
            temporary_cases_pointer_vector.size());
        std::ranges::copy(temporary_cases_pointer_vector, cases_pointer_slice.begin());

        switch_expression_node->Cases = cases_pointer_slice;

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

    Module *ParseModule()
    {
        auto *module = ObjectArenaAllocator.Allocate<Module>();

        while (!check_token_type_of_current_token(TokenTypes::EndOfFile))
        {
            std::print(
                "Parsing token: {} | TokenType: {} | PeekToken: {} | PeekTokenType: {}\n",
                CurrentToken.ObjectSourceLocation.Source,
                std::to_underlying(CurrentToken.TokenType),
                PeekToken.ObjectSourceLocation.Source,
                std::to_underlying(PeekToken.TokenType));
            switch (CurrentToken.TokenType)
            {
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
            }
            default:
            {
                ObjectDiagnosticEngine.Report(
                    Severity::Error,
                    UNRECOGNIZED_TOP_LEVEL_NODE,
                    CurrentToken.ObjectSourceLocation,
                    "Unrecognized top level token.",
                    "You sure don't look like you'd get very far on your wits.");
                advance_one_token(); // consume unknown token.
            }
            }
        }

        return module;
    }
}; // namespace AST

} // namespace AST
} // namespace JSlang
