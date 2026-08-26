#pragma once
#include "../cache/Libraries/include/enkits/enkiTS/TaskScheduler.h"
#include "TerminalTextStyling.hpp"
#include "string"
#include "vector"
#include <cstdint>
#include <cstdio>
#include <format>
#include <iostream>
#include <iterator>
#include <print>

namespace JSlang
{
struct ThreadUnsafeLogger
{
    enum class LogTypes : uint8_t
    {
        Info,
        Success,
        Warn,
        Error,
        Debug,
    };

    template <LogTypes LogType> static std::string FormatLogMessage()
    {
        namespace TTS = TerminalTextStyling;
        if constexpr (LogType == LogTypes::Info)
        {
            return std::format("{}[INFO]   : ", TTS::Foreground::BLUE);
        }
        else if constexpr (LogType == LogTypes::Success)
        {
            return std::format("{}[SUCCESS]: ", TTS::Foreground::GREEN);
        }
        else if constexpr (LogType == LogTypes::Warn)
        {
            return std::format("{}[WARN]   : ", TTS::Foreground::YELLOW);
        }
        else if constexpr (LogType == LogTypes::Error)
        {
            return std::format("{}[ERROR]  : ", TTS::Foreground::RED);
        }
        else
        {
            return std::format("[DEBUG]    : ");
        }
    }

    template <LogTypes LogType, typename... ArgumentTypes>
    static void Log(std::format_string<ArgumentTypes...> FormatString, ArgumentTypes &&...Arguments)
    {
        std::cout << ThreadUnsafeLogger::FormatLogMessage<LogType>();
        std::print(FormatString, std::forward<ArgumentTypes>(Arguments)...);
        std::cout << TerminalTextStyling::RESET;
    }

    template <LogTypes LogType, bool Verbose, typename... ArgumentTypes>
    static void
    LogVerbose(std::format_string<ArgumentTypes...> FormatString, ArgumentTypes &&...Arguments)
    {
        if constexpr (!Verbose)
        {
            return;
        }

        Log<LogType>(FormatString, std::forward<ArgumentTypes>(Arguments)...);
    }
};

struct ThreadSafeLogger
{
    using LogTypes = ThreadUnsafeLogger::LogTypes;

    std::vector<std::string> PrintBuffers;
    std::string              PrePrintOutputBuffer;

    enki::TaskScheduler *TaskScheduler;
    uint32_t             TotalCommittedThreads = 0;

    void
    Initialize(enki::TaskScheduler *ParameterTaskScheduler, uint32_t ParameterTotalCommittedThreads)
    {
        TaskScheduler         = ParameterTaskScheduler;
        TotalCommittedThreads = ParameterTotalCommittedThreads;

        constexpr uint32_t PrintBufferPreAllocationSize = 4096;

        PrePrintOutputBuffer.reserve(PrintBufferPreAllocationSize);

        PrintBuffers.resize(TotalCommittedThreads);
        for (auto &Buffer : PrintBuffers)
        {
            Buffer.reserve(PrintBufferPreAllocationSize / 2);
        }
    };

    void PrintLogBuffers()
    {
        PrePrintOutputBuffer.clear();

        uint32_t ThreadIndex = 0;
        for (auto &Buffer : PrintBuffers)
        {
            if (!Buffer.empty())
            {
                std::format_to(
                    std::back_insert_iterator(PrePrintOutputBuffer),
                    "{}|THREAD {}|{}\n{}",
                    TerminalTextStyling::Style::DIM,
                    ThreadIndex,
                    TerminalTextStyling::Style::DIM_OFF,
                    Buffer);
            }
            ++ThreadIndex;
        }

        if (!PrePrintOutputBuffer.empty())
        {
            std::print("{}", PrePrintOutputBuffer);
        }
    }

    void FlushAndClearBuffers()
    {
        std::fflush(stdout);
        for (auto &Buffer : PrintBuffers)
        {
            Buffer.clear();
        }
    }

    template <LogTypes LogType, typename... ArgumentTypes>
    void Log(std::format_string<ArgumentTypes...> FormatString, ArgumentTypes &&...Arguments)
    {
        auto thread_index = TaskScheduler->GetThreadNum();

        auto &print_buffer = PrintBuffers
            [(thread_index >= TotalCommittedThreads)
                 ? 0
                 : thread_index]; // Threads outside the task scheduler shouldn't be able to access
                                  // this. But if they do, there would be bigger fish to fry

        print_buffer.append(ThreadUnsafeLogger::FormatLogMessage<LogType>());
        print_buffer.append(std::format(FormatString, std::forward<ArgumentTypes>(Arguments)...));
        print_buffer.append(TerminalTextStyling::RESET);
    }

    template <LogTypes LogType, bool Verbose, typename... ArgumentTypes>
    void LogVerbose(std::format_string<ArgumentTypes...> FormatString, ArgumentTypes &&...Arguments)
    {
        if constexpr (!Verbose)
        {
            return;
        }

        Log<LogType>(FormatString, std::forward<ArgumentTypes>(Arguments)...);
    }
};
} // namespace JSlang
