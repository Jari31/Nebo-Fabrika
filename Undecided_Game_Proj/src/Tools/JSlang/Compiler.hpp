#pragma once

#include "CompilerTypes.hpp"
#include "Includes/Log.hpp"
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

    void CompileFromSource(CompileFromSourceRequest CompileRequest)
    {
        enki::TaskSet task(
            12000,
            [&](enki::TaskSetPartition Range, uint32_t ThreadIndex)
            { ThreadedLogger.Log<LogTypes::Info>("minecraft won't add inches to your co\n"); });

        ThreadedLogger.Log<LogTypes::Info>("Hello, world!\n");
        ThreadUnsafeLogger::Log<LogTypes::Info>("Hello, world!\n");

        TaskScheduler.AddTaskSetToPipe(&task);
        TaskScheduler.WaitforTask(&task);

        ThreadedLogger.PrintLogBuffers();
        ThreadedLogger.FlushAndClearBuffers();
    };
};
} // namespace JSlang
