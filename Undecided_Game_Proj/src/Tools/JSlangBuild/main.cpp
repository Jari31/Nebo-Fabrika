#include <cstddef>
#include <cstdint>
#include <format>
#include <vector>
#define TOML_EXCEPTIONS 0

#include "Includes/Parser.hpp"
#include "Includes/TerminalTextStyling.hpp"
#include "Libraries/include/CLI/CLI.hpp"
#include "Libraries/include/enkiTS/TaskScheduler.h"
#include <Libraries/include/toml++/toml.hpp>
#include <filesystem>
#include <print>
#include <string>

namespace filesystem = std::filesystem;

namespace JSlang
{
struct Build
{
    enki::TaskScheduler      TaskScheduler;
    std::vector<std::string> ShadersToCompilePaths;

    struct CLIOptions
    {
        bool     Verbose     = false;
        uint32_t ThreadCount = 0;
    } ObjectCLIOptions;

    void CreateSubCommand(CLI::App &ParentApp)
    {
        auto *subcommand =
            ParentApp.add_subcommand("build", "Build shaders listed within jslangbuild.toml");
        subcommand->add_flag("-v, --verbose", ObjectCLIOptions.Verbose);
        subcommand->add_option(
            "--thread_count",
            ObjectCLIOptions.ThreadCount,
            "How many threads to use in the building process. Recommended 2-4 for HDDs, and as "
            "many as you can spare for SSDs.");

        subcommand->callback(
            [this]()
            {
                if (ObjectCLIOptions.Verbose)
                {
                    BuildShaders<true>();
                }
                else
                {
                    BuildShaders<false>();
                }
            });
    }

    template <bool Verbose>
    void _glob_and_hash_files() {

    };

    template <bool Verbose> int BuildShaders()
    {
        if constexpr (Verbose)
        {
            std::print("Initializing TaskScheduler...\n");
        }

        if (ObjectCLIOptions.ThreadCount != 0)
        {
            ObjectCLIOptions.ThreadCount = std::thread::hardware_concurrency();
        }

        if constexpr (Verbose)
        {
            std::print("Building with {} thread(s).\n", ObjectCLIOptions.ThreadCount);
        }

        TaskScheduler.Initialize(ObjectCLIOptions.ThreadCount);

        if constexpr (Verbose)
        {
            std::print("TaskScheduler initialized.\n");
        }

        filesystem::path current_working_directory = filesystem::current_path();
        filesystem::path expected_file             = "jslangbuild.toml";

        auto path_to_expected_file = current_working_directory / expected_file;

        if (!filesystem::exists(path_to_expected_file))
        {
            using namespace TerminalTextStyling;

            std::print(
                "{}{}jslangbuild.toml{} not found.{} Checked for path: {}\n",
                Foreground::YELLOW,
                Style::UNDERLINE,
                Style::UNDERLINE_OFF,
                RESET,
                path_to_expected_file.string());

            return 0;
        }

        auto parsed_options = JSlang::Parser::ParseTOMLFile(expected_file);

        if (!parsed_options)
        {
            return -1;
        };

        return 0;
    };
};
} // namespace JSlang

int main(int ArgumentCount, char **ArgumentVector)
{
    CLI::App app{"A simple build system for Slang."};
    app.require_subcommand(1);

    JSlang::Build ObjectBuild;
    ObjectBuild.CreateSubCommand(app);

    CLI11_PARSE(app, ArgumentCount, ArgumentVector);

    return 0;
}
