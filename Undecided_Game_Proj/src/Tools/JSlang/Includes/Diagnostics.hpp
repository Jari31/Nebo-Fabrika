#pragma once

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
    uint32_t       ErrorCode;
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
        uint32_t       ErrorCode,
        SourceLocation SourceLocation,
        std::string    Message,
        std::string    Monologue,
        std::string    Hint = "")
    {
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

    [[nodiscard]] bool ContainsErrors() const { return ErrorCount > 0; }

    void Clear()
    {
        DiagnosticBuffer.clear();
        ErrorCount   = 0;
        WarningCount = 0;
    }
};
} // namespace JSlang
