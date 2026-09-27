#pragma once

#include "DebugHelpers.hpp"
#include "ErrorCodes.hpp"
#include "Libraries/include/enkits/enkiTS/TaskScheduler.h"
#include "Libraries/include/magic_enum/magic_enum.hpp"
#include "Log.hpp"
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace JSlang
{
enum class Severity : uint8_t
{
    Note,
    PerformanceWarning,
    Warning,
    Error,
    Fatal
};

struct SourceLocation
{
    std::string_view Source;
    uint64_t         SourceHash;

    std::string_view Filename;
    uint32_t         Line   = 0;
    uint32_t         Column = 0;
};

struct Diagnostic
{
    ErrorCodes     ErrorCode;
    Severity       Severity;
    std::string    Message;
    SourceLocation SourceLocation;
    std::string    Monologue;
    std::string    Hint;
};

struct DiagnosticEngine
{
    std::vector<std::vector<Diagnostic>> DiagnosticBuffers;
    uint32_t                             ErrorCount   = 0;
    uint32_t                             WarningCount = 0;

    enki::TaskScheduler *TaskScheduler = nullptr;

    DiagnosticEngine(enki::TaskScheduler *ParameterTaskScheduler)
    {
        TaskScheduler = ParameterTaskScheduler;
    }

    void Report(
        Severity       Severity,
        ErrorCodes     ErrorCode,
        SourceLocation SourceLocation,
        std::string    Message,
        std::string    Monologue,
        std::string    Hint = "")
    {
        SourceLocation.Line += 1;
        SourceLocation.Column += 1;

        auto thread_index = TaskScheduler->GetThreadNum();

        auto &diagnostic_buffer = DiagnosticBuffers[thread_index];

        diagnostic_buffer.push_back(
            {.ErrorCode      = ErrorCode,
             .Severity       = Severity,
             .Message        = std::move(Message),
             .SourceLocation = SourceLocation,
             .Monologue      = std::move(Monologue),
             .Hint           = std::move(Hint)});

        if (Severity == Severity::Error)
        {
            ++ErrorCount;
        }
        else if (Severity == Severity::Warning || Severity == Severity::PerformanceWarning)
        {
            ++WarningCount;
        }

        if (Severity == Severity::Fatal)
        {
            DebugHelpers::BuiltinTrap();
            std::exit(EXIT_FAILURE);
        }
    }

    // WARN not thread safe.
    template <bool ClearBuffer = true, bool FlushAfterWrite = false> void PrintBuffer()
    {
        for (auto &DiagnosticBuffer : DiagnosticBuffers)
        {
            for (auto &Diagnostic : DiagnosticBuffer)
            {
                ThreadUnsafeLogger::Log<ThreadUnsafeLogger::LogTypes::Info>(
                    "ISSUE WITH: {}, SEVERITY: {}, ERROR CODE: {}, MESSAGE: {}, LINE: {}, COLUMN: "
                    "{}\n",
                    std::string_view(
                        Diagnostic.SourceLocation.Source.data(),
                        Diagnostic.SourceLocation.Source.size()),
                    magic_enum::enum_name(Diagnostic.Severity),
                    magic_enum::enum_name(Diagnostic.ErrorCode),
                    Diagnostic.Message,
                    Diagnostic.SourceLocation.Line,
                    Diagnostic.SourceLocation.Column);
            }

            ThreadUnsafeLogger::Flush();

            if constexpr (ClearBuffer)
            {
                DiagnosticBuffers.clear();
            }
        }
    }

    [[nodiscard]] bool ContainsErrors() const { return ErrorCount > 0; }

    void Clear()
    {
        DiagnosticBuffers.clear();
        ErrorCount   = 0;
        WarningCount = 0;
    }
};
} // namespace JSlang
