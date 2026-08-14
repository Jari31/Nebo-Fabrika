#pragma once

#include <format>
#include <functional>
#include <iterator>
#include <utility>
#define TOML_EXCEPTIONS 0

#include "FileIO.hpp"
#include "HelperFunctions.hpp"
#include "Libraries/include/toml++/toml.hpp"
#include "Math.hpp"
#include "TerminalTextStyling.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace JSlang::Parser
{
namespace filesystem = std::filesystem;

enum class Errors : uint8_t
{
    ParserError,
};

struct ParsedOptions
{
    std::vector<std::string> SearchFolders;
    filesystem::path         OutputDirectory;
};

inline std::expected<ParsedOptions, Errors> ParseTOMLFile(const filesystem::path &BuildFilePath)
{
    using LogTypes    = HelperFunctions::LogTypes;
    auto parse_result = toml::parse_file(BuildFilePath.string());

    if (parse_result
            .failed()) // WARN this is a proof of concept; improve and standardize this once open
    {
        namespace TTS            = JSlang::TerminalTextStyling;
        const auto &error_source = parse_result.error().source();

        auto failed_to_parse_line_from = error_source.begin.line;
        auto failed_to_parse_line_to   = error_source.end.line;

        auto lines = FileIO::ReadFileSequential(
            BuildFilePath, failed_to_parse_line_from, failed_to_parse_line_to);

        failed_to_parse_line_to -= failed_to_parse_line_from;
        failed_to_parse_line_from = 0;
        { // IMPORTANT refactor this into another inline function
            std::string  string_to_find = "\n";
            unsigned int count          = 0;
            auto         search_algorithm =
                std::boyer_moore_searcher(string_to_find.begin(), string_to_find.end());

            auto iterator = lines.begin();
            while ((iterator = std::search(iterator, lines.end(), search_algorithm)) != lines.end())
            {
                count++;
                std::advance(iterator, string_to_find.length());
            }

            if (count > 0)
            {
                std::string gutter_whitespaces;

                for (unsigned int i = 0;
                     i < Math::CountDigits<unsigned int>(error_source.begin.line);
                     i++)
                {
                    gutter_whitespaces += " ";
                }

                std::string error_visualization_string = gutter_whitespaces + "|";
                for (unsigned int i = 1; i < error_source.begin.column; i++)
                {
                    error_visualization_string += " ";
                }

                for (unsigned int i = error_source.begin.column; i <= error_source.end.column; i++)
                {
                    error_visualization_string += "^";
                }

                lines += error_visualization_string +
                         std::format(" (Column {})", error_source.end.column);
            }
        }
        // INFO seems to only err out a single line; so assuming that...
        HelperFunctions::Log<LogTypes::Error>(
            "Failed to parse '{}' with error:\n"
            "BEGIN : LINE {} COLUMN {}\n\n"
            "{}{}|{}{}\n"
            "{}\n\n"
            "END : LINE {} COLUMN {}{}\n",
            filesystem::absolute(BuildFilePath).string(),
            error_source.begin.line,
            error_source.begin.column,
            TTS::Foreground::BRIGHT_BLACK,
            error_source.begin.line,
            lines,
            TTS::Foreground::RED,
            parse_result.error().description(),
            error_source.end.line,
            error_source.end.column,
            TTS::RESET);

        return std::unexpected(Errors::ParserError);
    }

    auto          table = std::move(parse_result).table();
    ParsedOptions parsed_options;

    if (auto *file_paths = table["Build"]["SearchFolders"].as_array())
    {
        parsed_options.SearchFolders.reserve(file_paths->size());

        for (auto &&element : *file_paths)
        {
            if (auto path = element.value<std::string>())
            {
                parsed_options.SearchFolders.push_back(*path);
            }
        }
    }

    parsed_options.OutputDirectory = table["Build"]["OutputFolder"].value_or("build");

    return parsed_options;
}
} // namespace JSlang::Parser
