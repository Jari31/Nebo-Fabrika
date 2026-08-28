#pragma once

#include "CompilerTypes.hpp"
#include "Diagnostics.hpp"
#include "Includes/Log.hpp"
#include "Lexer.hpp"
#include "cache/Libraries/include/enkits/enkiTS/TaskScheduler.h"
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

    CompileResult CompileFromSource(CompileFromSourceRequest CompileRequest)
    {
        DiagnosticEngine ObjectDiagnosticEngine;
        Lexer            lexer(
            ObjectDiagnosticEngine, CompileRequest.SourceCode, CompileRequest.SourceFileName);

        while (true)
        {
            Token current_token = lexer.GetNextToken();

            ThreadUnsafeLogger::Log<LogTypes::Info>(
                "[TOKEN_TYPE: {} | TOKEN_BODY: {} | LINE: {} | COLUMN: {}]\n",
                uint32_t(current_token.TokenType),
                current_token.ObjectSourceLocation.Source,
                current_token.ObjectSourceLocation.Line,
                current_token.ObjectSourceLocation.Column);
            if (current_token.TokenType == TokenTypes::Invalid ||
                current_token.TokenType == TokenTypes::EndOfFile)
            {
                break;
            }
        }

        return {};
    };
};
} // namespace JSlang
