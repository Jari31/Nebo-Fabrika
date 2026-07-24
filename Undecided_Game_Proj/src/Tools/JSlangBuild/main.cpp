
#include <cstdint>
#include <expected>
#include <optional>
#define TOML_EXCEPTIONS 0
#define TOML_ENABLE_FORMATTERS 1

#include "Libraries/include/CLI/CLI.hpp"
#include "TerminalTextStyling.hpp"
#include <Libraries/include/toml++/toml.hpp>
#include <filesystem>
#include <print>

namespace filesystem = std::filesystem;

namespace JSlangBuild::Parser
{
enum class Errors : uint8_t
{
    ParserError,
};

struct ParsedOptions
{
    std::vector<std::string> Paths;
};

inline std::expected<ParsedOptions, Errors> ParseTOMLFile(const filesystem::path &BuildFilePath)
{
    auto parse_result = toml::parse_file(BuildFilePath.string());

    if (parse_result.failed())
    {
        const auto &error_source = parse_result.error().source();

        std::print(
            "{}Failed to parse jslangbuild.toml with error: {}\n| at |\nBEGIN : LINE {} COLUMN "
            "{}\nEND : LINE {} COLUMN {}{}\n",
            TerminalTextStyling::Foreground::RED,
            parse_result.error().description(),
            error_source.begin.line,
            error_source.begin.column,
            error_source.end.line,
            error_source.end.column,
            TerminalTextStyling::RESET);

        return std::unexpected(Errors::ParserError);
    }

    auto          table = std::move(parse_result).table();
    ParsedOptions parsed_options;

    if (auto *file_paths = table["SlangFilePaths"]["tags"].as_array())
    {
        parsed_options.Paths.reserve(file_paths->size());

        for (auto &&element : *file_paths)
        {
            if (auto path = element.value<std::string>())
            {
                parsed_options.Paths.push_back(*path);
            }
        }
    }

    return parsed_options;
}
} // namespace JSlangBuild::Parser

int main(int ArgumentCount, char **ArgumentVector)
{
    CLI::App app{"A simple build system for Slang."};

    CLI11_PARSE(app, ArgumentCount, ArgumentVector);

    filesystem::path current_working_directory = filesystem::current_path();
    filesystem::path expected_file             = "jslangbuild.toml";

    auto path_to_expected_file = current_working_directory / expected_file;

    if (!filesystem::exists(path_to_expected_file))
    {
        using namespace TerminalTextStyling;

        std::print(
            "{}{}jslangbuild.toml{} not found.{} Checked for string: {}\n",
            Foreground::YELLOW,
            Style::UNDERLINE,
            Style::UNDERLINE_OFF,
            RESET,
            path_to_expected_file.string());

        return 0;
    }

    auto parsed_options = JSlangBuild::Parser::ParseTOMLFile(expected_file);

    if (!parsed_options)
    {
        return -1;
    };

    return 0;
}
