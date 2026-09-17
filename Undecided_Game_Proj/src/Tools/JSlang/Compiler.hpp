#pragma once

// #include "ASTParser.hpp"
#include "ASTParser.hpp"
#include "ArenaAllocator.hpp"
#include "CompilerTypes.hpp"
#include "Diagnostics.hpp"
#include "Includes/Log.hpp"
#include "Lexer.hpp"
#include "Libraries/include/enkits/enkiTS/TaskScheduler.h"
#include "SupportedEmbeddedLanguagesEnum.hpp"
#include <cstdint>

namespace JSlang
{

struct Compiler
{
    using LogTypes = ThreadSafeLogger::LogTypes;

    enki::TaskScheduler TaskScheduler;
    ThreadSafeLogger    ThreadedLogger;
    ThreadUnsafeLogger  Logger;

    bool Verbose = false;

    void Initialize(CompilerInitializationOptions Options)
    {
        TaskScheduler.Initialize(Options.CompileWithThreads);
        Verbose = Options.Verbose;

        ThreadedLogger.Initialize(&TaskScheduler, Options.CompileWithThreads);
    }

    static CompileResult CompileFromSource(CompileFromSourceRequest CompileRequest)
    {
        EmbeddedLanguageCodeblocks embedded_language_codeblocks;
        embedded_language_codeblocks.resize(1);
        DiagnosticEngine diagnostic_engine;
        Lexer            lexer(
            diagnostic_engine,
            embedded_language_codeblocks,
            CompileRequest.SourceCode,
            CompileRequest.SourceFileName);

        ArenaAllocator arena_allocator;
        AST::Parser    parser(lexer, arena_allocator);

        // while (true)
        // {
        //     Token current_token = lexer.GetNextToken();

        //     ThreadUnsafeLogger::Log<LogTypes::Info>(
        //         "[TOKEN_TYPE: {} | TOKEN_BODY: {} | LINE: {} | COLUMN: {}]\n",
        //         uint32_t(current_token.TokenType),
        //         current_token.ObjectSourceLocation.Source,
        //         current_token.ObjectSourceLocation.Line,
        //         current_token.ObjectSourceLocation.Column);
        //     if (current_token.TokenType == TokenTypes::Invalid ||
        //         current_token.TokenType == TokenTypes::EndOfFile)
        //     {
        //         break;
        //     }
        // }

        auto *module = parser.ParseModule();

        for (auto *node : module->TopLevelNodes)
        {
            ThreadUnsafeLogger::Log<LogTypes::Info>(
                "[SOURCE = {}, FILENAME = {}, LINE = {}, COLUMN = {}]\n",
                node->ObjectSourceLocation.Source,
                node->ObjectSourceLocation.Filename,
                node->ObjectSourceLocation.Line,
                node->ObjectSourceLocation.Column);
        }

        return {};
    };
};
} // namespace JSlang
