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
    uint32_t SourceHandle;
    uint32_t Line_Column = 0;

    template <bool ClearBeforeAssign = false, bool AssumeValueIsInRange = true>
    void SetLine(uint32_t Value)
    {
        if constexpr (ClearBeforeAssign)
        {
            Line_Column &= ~(0xFFFFF << 12);
        }

        // if this somehow goes over 1 million lines, the guy who wrote the shit behind this
        // bullshit genuinely just needs to just end it all. i swear
        if constexpr (AssumeValueIsInRange)
        {
            Line_Column |= Value << 12;
            return;
        }

        Line_Column |= (Value & 0xFFFFF) << 12;
    }

    template <bool ClearBeforeAssign = false> void SetColumn(uint32_t Value)
    {
        if constexpr (ClearBeforeAssign)
        {
            Line_Column &= ~0xFFF;
        }
        Line_Column |= Value & 0xFFF;
    }

    [[nodiscard]] constexpr uint32_t GetLine() const { return Line_Column >> 12; }
    [[nodiscard]] constexpr uint32_t GetColumn() const { return Line_Column & 0xFFF; }
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

    // unused during the untyped phase
    std::string_view Filepath = "UNDEFINED";

    enki::TaskScheduler *TaskScheduler = nullptr;

    void Report(
        Severity       Severity,
        ErrorCodes     ErrorCode,
        SourceLocation SourceLocation,
        std::string    Message,
        std::string    Monologue,
        std::string    Hint = "")
    {
        SourceLocation.SetLine<true>((SourceLocation.GetLine() + 1));
        SourceLocation.SetColumn<true>((SourceLocation.GetColumn() + 1));

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
                    Diagnostic.SourceLocation.GetLine(),
                    Diagnostic.SourceLocation.GetColumn());
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
