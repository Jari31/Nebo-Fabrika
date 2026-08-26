#pragma once
#include "TerminalTextStyling.hpp"
#include <filesystem>
#include <fstream>
#include <print>
#include <source_location>
#include <string>

namespace JSlang::FileIO
{
namespace filesystem = std::filesystem;

// NOTE: Refactor this to use Log.hpp logging
// NOTE: This is awful. It works, but replace it with a memmap implementation when time's on hand
std::string ReadFileSequential(
    const filesystem::path    &FilePath,
    unsigned int               StartFromLine,
    unsigned int               StopAtLine,
    const std::source_location InvokerLocation = std::source_location::current())
{
    auto lambda_print_invoker_information = [=]()
    {
        std::print(
            "FILE_NAME: {}\n FUNCTION_NAME: {}\n LINE {} COLUMN {}",
            InvokerLocation.file_name(),
            InvokerLocation.function_name(),
            InvokerLocation.line(),
            InvokerLocation.column());
    };

    if (StartFromLine < 0 || StopAtLine <= 0 || StopAtLine < StartFromLine)
    {
        namespace TTS = TerminalTextStyling;
        std::print(
            "{}Invalid arguments provided for file reading.{}\n", TTS::Foreground::RED, TTS::RESET);
        lambda_print_invoker_information();
        return "";
    }

    if (!filesystem::exists(FilePath))
    {
        namespace TTS = TerminalTextStyling;
        std::print(
            "{}File '{}' not found.{}\n", TTS::Foreground::RED, FilePath.string(), TTS::RESET);
        lambda_print_invoker_information();
        return "";
    }

    std::ifstream file(FilePath);
    if (!file)
    {
        namespace TTS = TerminalTextStyling;
        std::print(
            "{}Failed to open file '{}'.{}\n", TTS::Foreground::RED, FilePath.string(), TTS::RESET);
        lambda_print_invoker_information();
        return "";
    }

    std::string  lines;
    std::string  line;
    unsigned int current_line = 1;

    while (std::getline(file, line))
    {
        if (current_line >= StartFromLine)
        {
            lines.append(line).append("\n");
        }
        if (current_line >= StopAtLine)
        {
            return lines;
        }
        current_line++;
    }

    {
        namespace TTS = TerminalTextStyling;
        std::print(
            "{}Read file '{}' but found nothing inside.{}\n",
            TTS::Foreground::YELLOW,
            FilePath.string(),
            TTS::RESET);
    }

    return "";
}
} // namespace JSlang::FileIO
