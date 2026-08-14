#pragma once

#include "Libraries/include/whereami.h"
#include "TerminalTextStyling.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <print>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace JSlang::HelperFunctions
{
namespace filesystem = std::filesystem;

enum class LogTypes : uint8_t
{
    Info,
    Success,
    Warn,
    Error,
    Debug,
};

/// Verbose has no effect if LogOnlyIfVerbose is set to false.
template <
    auto LogType          = LogTypes::Info,
    bool LogOnlyIfVerbose = false,
    bool Verbose          = false,
    typename... ArgumentsType>
void Log(std::format_string<ArgumentsType...> FormatString, ArgumentsType &&...Arguments)
{
    namespace TTS = TerminalTextStyling;

    if constexpr (LogOnlyIfVerbose && !Verbose)
    {
        return;
    }

    if constexpr (LogType == LogTypes::Info)
    {
        std::print("{}[INFO]: ", TTS::Foreground::BLUE);
    }
    else if constexpr (LogType == LogTypes::Success)
    {
        std::print("{}[SUCCESS]: ", TTS::Foreground::GREEN);
    }
    else if constexpr (LogType == LogTypes::Warn)
    {
        std::print("{}[WARN]: ", TTS::Foreground::YELLOW);
    }
    else if constexpr (LogType == LogTypes::Error)
    {
        std::print("{}[ERROR]: ", TTS::Foreground::RED);
    }
    else if constexpr (LogType == LogTypes::Debug)
    {
        std::print("[DEBUG]: ");
    }

    std::print(FormatString, std::forward<ArgumentsType>(Arguments)...);

    if constexpr (LogType != LogTypes::Debug)
    {
        std::print("{}", TTS::RESET);
    }
}

constexpr std::string GetDLLExtensionForTargetPlatform()
{
#if defined(__APPLE__)
    return ".dylib";
#elif defined(_WIN32)
    return ".dll";
#else
    return ".so";
#endif
}

std::filesystem::path GetInstallationDirectory()
{
    int string_buffer_length = wai_getExecutablePath(NULL, 0, NULL); // NOLINT
    if (string_buffer_length <= 0)
    {
        Log<LogTypes::Error>(
            "Failed to get the installation directory. Maybe you installed it in a directory where "
            "the application lacks permission to access things? Or perhaps you're in safe mode?");
        return {};
    }

    std::vector<char> installation_path_buffer(string_buffer_length + 1);
    int               directory_name_length = 0;
    wai_getExecutablePath(
        installation_path_buffer.data(), string_buffer_length, &directory_name_length);

    installation_path_buffer[directory_name_length] = '\0';

    return {installation_path_buffer.data()};
}

template <typename Type> Type GetFileLastWriteTimeAsType(const filesystem::path &FilePath)
{
    auto file_last_write_time = filesystem::last_write_time(FilePath);

    auto system_time = std::chrono::clock_cast<std::chrono::system_clock>(file_last_write_time);

    auto seconds_since_epoch =
        std::chrono::duration_cast<std::chrono::seconds>(system_time.time_since_epoch()).count();

    return static_cast<Type>(seconds_since_epoch);
}

} // namespace JSlang::HelperFunctions
