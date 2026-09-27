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
        DiagnosticEngine diagnostic_engine;
        Lexer lexer(diagnostic_engine, CompileRequest.SourceCode, CompileRequest.SourceFileName);

        ArenaAllocator arena_allocator;
        AST::Parser    parser(lexer, arena_allocator);

        auto *module = parser.ParseModule();

        return {};
    };
};

} // namespace JSlang
