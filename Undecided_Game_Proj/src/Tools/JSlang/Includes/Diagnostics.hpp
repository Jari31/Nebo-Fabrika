#pragma once

#include "ErrorCodes.hpp"
#include "Libraries/include/magic_enum/magic_enum.hpp"
#include "Log.hpp"
#include <cstdint>
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
    std::vector<Diagnostic> DiagnosticBuffer;
    uint32_t                ErrorCount   = 0;
    uint32_t                WarningCount = 0;

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

        DiagnosticBuffer.push_back(
            {.ErrorCode      = ErrorCode,
             .Severity       = Severity,
             .Message        = std::move(Message),
             .SourceLocation = SourceLocation,
             .Monologue      = std::move(Monologue),
             .Hint           = std::move(Hint)});

        if (Severity == Severity::Error || Severity == Severity::Fatal)
        {
            ++ErrorCount;
        }
        else if (Severity == Severity::Warning || Severity == Severity::PerformanceWarning)
        {
            ++WarningCount;
        }
    }

    void PrintBuffer()
    {
        for (auto &Diagnostic : DiagnosticBuffer)
        {
            ThreadUnsafeLogger::Log<ThreadUnsafeLogger::LogTypes::Info>(
                "ISSUE WITH: {}, SEVERITY: {}, ERROR CODE: {}, MESSAGE: {}, LINE: {}, COLUMN: {}\n",
                Diagnostic.SourceLocation.Source,
                magic_enum::enum_name(Diagnostic.Severity),
                magic_enum::enum_name(Diagnostic.ErrorCode),
                Diagnostic.Message,
                Diagnostic.SourceLocation.Line,
                Diagnostic.SourceLocation.Column);
        }

        DiagnosticBuffer.clear();
    }

    [[nodiscard]] bool ContainsErrors() const { return ErrorCount > 0; }

    void Clear()
    {
        DiagnosticBuffer.clear();
        ErrorCount   = 0;
        WarningCount = 0;
    }
};
} // namespace JSlang
