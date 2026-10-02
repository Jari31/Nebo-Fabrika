#pragma once

#include "ArenaAllocator.hpp"
#include "DebugHelpers.hpp"
#include "ErrorCodes.hpp"
#include "Libraries/include/enkits/enkiTS/TaskScheduler.h"
#include "Libraries/include/magic_enum/magic_enum.hpp"
#include "Log.hpp"
#include "StringInterner.hpp"
#include <algorithm>
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
    static constexpr uint32_t MAX_LINES = 0xFFFFFU;
    static constexpr uint32_t MAX_COLUM = 0xFFFU;

    ArenaAllocator::Handle SourceHandle = 0;
    uint32_t               Line_Column  = 0;

    void SetLine(uint32_t Value)
    {
        Line_Column &= ~(MAX_LINES << 12);

        Value = std::min(Value, MAX_LINES);

        Line_Column |= Value << 12;
    }

    void SetColumn(uint32_t Value)
    {
        Line_Column &= ~MAX_COLUM;
        Line_Column |= std::min(Value, MAX_COLUM);
    }

    [[nodiscard]] constexpr uint32_t GetLine() const { return Line_Column >> 12; }
    [[nodiscard]] constexpr uint32_t GetColumn() const { return Line_Column & 0xFFFU; }
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

    std::string_view Filepath = "UNDEFINED";
    StringInterner  &ObjectStringInternet;

    DiagnosticEngine(StringInterner &ParameterStringInterner)
        : ObjectStringInternet(ParameterStringInterner)
    {
    }

    void Report(
        Severity       Severity,
        ErrorCodes     ErrorCode,
        SourceLocation SourceLocation,
        std::string    Message,
        std::string    Monologue,
        std::string    Hint = "")
    {
        SourceLocation.SetLine((SourceLocation.GetLine() + 1));
        SourceLocation.SetColumn((SourceLocation.GetColumn() + 1));

        DiagnosticBuffer.push_back(
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
        for (auto &Diagnostic : DiagnosticBuffer)
        {
            auto string = ObjectStringInternet.GetStringViewOverHandle(
                Diagnostic.SourceLocation.SourceHandle);

            ThreadUnsafeLogger::Log<ThreadUnsafeLogger::LogTypes::Info>(
                "ISSUE WITH: {}, SEVERITY: {}, ERROR CODE: {}, MESSAGE: {}, LINE: {}, "
                "COLUMN: "
                "{}\n",
                string,
                magic_enum::enum_name(Diagnostic.Severity),
                magic_enum::enum_name(Diagnostic.ErrorCode),
                Diagnostic.Message,
                Diagnostic.SourceLocation.GetLine(),
                Diagnostic.SourceLocation.GetColumn());
        }

        ThreadUnsafeLogger::Flush();

        if constexpr (ClearBuffer)
        {
            DiagnosticBuffer.clear();
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
