#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>
#define TOML_EXCEPTIONS 0

#include "Includes/DirectoryWalker.hpp"
#include "Includes/HashMap.hpp"
#include "Includes/HelperFunctions.hpp"
#include "Includes/Parser.hpp"
#include "Includes/TerminalTextStyling.hpp"
#include "Libraries/include/CLI/CLI.hpp"
#include "Libraries/include/ankerl/unordered_dense.h"
#include "Libraries/include/dylib.hpp"
#include "Libraries/include/enkiTS/TaskScheduler.h"
#include "Libraries/include/slang-com-ptr.h"
#include "Libraries/include/slang.h"
#include <Libraries/include/toml++/toml.hpp>

#define EXTERNAL_CACHE_DIRECTORY "../../cache/"
#define CACHE_DIRECTORY "cache/"

#ifndef JSLANG_VERSION
#define JSLANG_VERSION "jslang-v0.1"
#endif

namespace filesystem = std::filesystem;

namespace JSlang
{
namespace DynamicLibraryLoader
{
enum class Errors : uint8_t
{
    FailedToCreateCacheDirectories,
    FailedToOpenCompanionDLLFile,
    FailedToReadCompanionDLLFile,
    FailedToWriteCompanionDLLFile,
    FailedToCloseCompanionDLLFile,
};

using DynamicLibraryMap = ankerl::unordered_dense::map<std::string, std::span<const uint8_t>>;

template <bool Verbose = false>
std::expected<bool, Errors> CacheCompanionDynamicLibraries(
    const filesystem::path &PathToCacheDirectory,
    DynamicLibraryMap      &LoadDyLibFromFilePaths)
{
    using LogTypes = HelperFunctions::LogTypes;

    for (const auto &[dylib_name, dynamic_library_bytes] : LoadDyLibFromFilePaths)
    {
        filesystem::path dylib_path = PathToCacheDirectory / dylib_name;

        if (!filesystem::exists(dylib_path))
        {
            HelperFunctions::Log<LogTypes::Warn, true, Verbose>(
                "Companion dynamic library file not found: {}. Emitting from memory...\n",
                dylib_name);

            std::ofstream dylib_file(dylib_path, std::ios::binary);

            if (!dylib_file)
            {
                HelperFunctions::Log<LogTypes::Error>(
                    "Failed to open companion dynamic library file. Maybe there isn't enough "
                    "space? Maybe you forgot to give the program proper permissions?\n");
                return std::unexpected(Errors::FailedToOpenCompanionDLLFile);
            }

            dylib_file.write(
                reinterpret_cast<const char *>(dynamic_library_bytes.data()),
                dynamic_library_bytes.size()); // NOLINT

            if (!dylib_file.is_open())
            {
                HelperFunctions::Log<LogTypes::Error>(
                    "Failed to write to companion dynamic library file. Maybe there isn't enough "
                    "space? Maybe the program doesn't have proper permissions?\n");
                return std::unexpected(Errors::FailedToWriteCompanionDLLFile);
            }

            dylib_file.close();

            if (dylib_file.bad())
            {
                HelperFunctions::Log<LogTypes::Error>(
                    "Failed to close companion dynamic library file.\n");
                return std::unexpected(Errors::FailedToCloseCompanionDLLFile);
            }
        }
    }

    return true;
}

inline std::expected<dylib::library, Errors>
LoadDynamicLibrary(const filesystem::path &PathToDynamicLibrary)
{
    using LogTypes = HelperFunctions::LogTypes;
    try
    {
        dylib::library dynamic_library(
            PathToDynamicLibrary.string(), dylib::decorations::os_default());

        return dynamic_library;
    }
    catch (const dylib::load_error &Error)
    {
        HelperFunctions::Log<LogTypes::Error>(
            "Failed to load slang-compiler dynamic library (maybe the program doesn't have "
            "sufficient permissions? Maybe there isn't enough RAM? Maybe the dynamic library "
            "doesn't exist?):  {}\n",
            Error.what());
        return std::unexpected(Errors::FailedToOpenCompanionDLLFile);
    }
}

}; // namespace DynamicLibraryLoader

#ifndef PATH_TO_SLANG_COMPILER_DYNAMIC_LIBRARY
#define PATH_TO_SLANG_COMPILER_DYNAMIC_LIBRARY "../../cache/Libraries/lib/slang-compiler.dll"
#endif

#ifndef PATH_TO_SLANG_GLSL_MODULE_DYNAMIC_LIBRARY
#define PATH_TO_SLANG_GLSL_MODULE_DYNAMIC_LIBRARY "../../cache/Libraries/lib/slang-glsl-module.dll"
#endif

#ifndef PATH_TO_SLANG_GL_SLANG_DYNAMIC_LIBRARY
#define PATH_TO_SLANG_GL_SLANG_DYNAMIC_LIBRARY "../../cache/Libraries/lib/slang-glslang.dll"
#endif

#ifndef PATH_TO_SLANG_LLVM_DYNAMIC_LIBRARY
#define PATH_TO_SLANG_LLVM_DYNAMIC_LIBRARY "../../cache/Libraries/lib/slang-llvm.dll"
#endif

#ifndef PATH_TO_SLANG_RUN_TIME_DYNAMIC_LIBRARY
#define PATH_TO_SLANG_RUN_TIME_DYNAMIC_LIBRARY "../../cache/Libraries/lib/slang-rt.dll"
#endif

#ifdef __clangd__
// Feed clangd a single dummy byte so it doesn't try to parse 100MB of DLLs
constexpr uint8_t DYNAMIC_LIBRARY_SLANG_COMPILER[] = {0x00};
#else
constexpr uint8_t DYNAMIC_LIBRARY_SLANG_COMPILER[] = {
#embed PATH_TO_SLANG_COMPILER_DYNAMIC_LIBRARY // NOLINT
};
#endif

#ifdef __clangd__
constexpr uint8_t DYNAMIC_LIBRARY_SLANG_GLSL_MODULE[] = {0x00};
#else
constexpr uint8_t DYNAMIC_LIBRARY_SLANG_GLSL_MODULE[] = {
#embed PATH_TO_SLANG_GLSL_MODULE_DYNAMIC_LIBRARY // NOLINT
};
#endif

#ifdef __clangd__
constexpr uint8_t DYNAMIC_LIBRARY_SLANG_GL_SLANG[] = {0x00};
#else
constexpr uint8_t DYNAMIC_LIBRARY_SLANG_GL_SLANG[] = {
#embed PATH_TO_SLANG_GL_SLANG_DYNAMIC_LIBRARY // NOLINT
};
#endif

#ifdef __clangd__
constexpr uint8_t DYNAMIC_LIBRARY_SLANG_LLVM[] = {0x00};
#else
constexpr uint8_t DYNAMIC_LIBRARY_SLANG_LLVM[] = {
#embed PATH_TO_SLANG_LLVM_DYNAMIC_LIBRARY // NOLINT
};
#endif

#ifdef __clangd__
constexpr uint8_t DYNAMIC_LIBRARY_SLANG_RUN_TIME[] = {0x00};
#else
constexpr uint8_t DYNAMIC_LIBRARY_SLANG_RUN_TIME[] = {
#embed PATH_TO_SLANG_RUN_TIME_DYNAMIC_LIBRARY // NOLINT
};
#endif

struct Build
{

    using LogTypes = HelperFunctions::LogTypes;

    enki::TaskScheduler      TaskScheduler;
    std::vector<std::string> ShadersToCompilePaths;
    filesystem::path         TemporaryCacheDirectory;

    enum class Error : uint8_t
    {
        FailedToFindDirectory,
    };

    struct CLIOptions
    {
        bool     Verbose     = false;
        uint32_t ThreadCount = 0;
    } ObjectCLIOptions;

    void CreateSubCommand(CLI::App &ParentApp)
    {
        auto *subcommand =
            ParentApp.add_subcommand("build", "Build shaders listed within jslang.toml");

        subcommand->add_option(
            "--thread_count",
            ObjectCLIOptions.ThreadCount,
            "How many threads to use in the building process. "
            "Recommended 2-4 for HDDs, and as "
            "many as you can spare for SSDs.");

        subcommand->add_flag("-v, --verbose", ObjectCLIOptions.Verbose);

        subcommand->callback(
            [this]()
            {
                auto build_start_time = std::chrono::high_resolution_clock::now();
                if (ObjectCLIOptions.Verbose)
                {
                    BuildShaders<true>();
                }
                else
                {
                    BuildShaders<false>();
                }
                auto build_end_time = std::chrono::high_resolution_clock::now();
                auto build_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                          build_end_time - build_start_time)
                                          .count();

                HelperFunctions::Log<LogTypes::Info>("Build process took {}ms.\n", build_duration);
            });
    }

    static std::expected<uint64_t, Error> hash_file_date_and_size(const filesystem::path &FilePath)
    {
        if (!filesystem::exists(FilePath))
        {
            return std::unexpected(Error::FailedToFindDirectory);
        }

        uint64_t file_size        = filesystem::file_size(FilePath);
        auto file_last_write_time = HelperFunctions::GetFileLastWriteTimeAsType<uint64_t>(FilePath);

        uint64_t hash_input = file_size ^ file_last_write_time;
        return XXH64(&hash_input, sizeof(hash_input), 0);
    }

    static std::vector<filesystem::path> check_module_dependencies(slang::IModule *Module)
    {
        auto dependency_count = Module->getDependencyFileCount();
        for (uint32_t i = 0; i < dependency_count; i++)
        {
        }
    }

    static int _compile_shaders(
        enki::TaskScheduler                                   &TaskScheduler,
        std::vector<filesystem::path>                         &ShaderFilePathsToCheck,
        const Slang::ComPtr<slang::IGlobalSession>            &GlobalSession,
        std::vector<Parser::ParsedOptions::TargetDescription> &TargetDescriptions,
        const filesystem::path                                &OutputFolder)
    {
        // if (!filesystem::exists(".jslang/build_manifest.bin"))
        // {
        //     return ShaderFilePathsToCheck;
        // }
        // auto hash_map = HashMap::LoadHashMapFromDisk(".jslang/build_manifest.bin");

        auto thread_count = TaskScheduler.GetNumTaskThreads();

        std::vector<std::string> thread_global_shader_compilation_diagnostic_info;
        thread_global_shader_compilation_diagnostic_info.reserve(thread_count);

        auto parse_target_format = [&](std::string &Format) -> SlangCompileTarget
        {
            std::ranges::transform(Format, Format.begin(), ::tolower);

            static const ankerl::unordered_dense::map<std::string_view, SlangCompileTarget>
                target_map = {
                    {"spirv", SLANG_SPIRV},
                    {"spir-v", SLANG_SPIRV},
                    {"dxil", SLANG_DXIL},
                    {"hlsl", SLANG_HLSL},
                    {"glsl", SLANG_GLSL},
                    {"cuda", SLANG_CUDA_SOURCE},
                    {"cpp", SLANG_CPP_SOURCE},
                    {"c++", SLANG_CPP_SOURCE},
                    {"wgsl", SLANG_WGSL}};

            auto iterator = target_map.find(Format);
            if (iterator != target_map.end())
            {
                return iterator->second;
            }

            return SLANG_TARGET_UNKNOWN;
        };

        std::vector<std::vector<filesystem::path>> thread_global_shader_compilation_list(
            thread_count);
        enki::TaskSet task(
            ShaderFilePathsToCheck.size(),
            [&](enki::TaskSetPartition Range, uint32_t ThreadIndex) -> void
            {
                Slang::ComPtr<slang::ISession> session;
                slang::SessionDesc             session_description = {};

                std::vector<slang::TargetDesc> target_descriptions;
                target_descriptions.reserve(TargetDescriptions.size());

                uint32_t target_index = 0;
                for (auto &TargetDescription : TargetDescriptions)
                {
                    auto &target_description = target_descriptions[target_index];

                    target_description        = {};
                    target_description.format = parse_target_format(TargetDescription.Format);
                    target_description.profile =
                        GlobalSession->findProfile(TargetDescription.Profile.c_str());

                    target_description.flags = 0;
                    if (TargetDescription.GenerateSPIRVDirectly)
                    {
                        target_description.flags |= SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
                    }
                    if (TargetDescription.GenerateWholeProgram)
                    {
                        target_description.flags |= SLANG_TARGET_FLAG_GENERATE_WHOLE_PROGRAM;
                    }
                }

                session_description.targets     = target_descriptions.data();
                session_description.targetCount = static_cast<SlangInt>(target_descriptions.size());

                if (SLANG_FAILED(
                        GlobalSession->createSession(session_description, session.writeRef())))
                {
                    HelperFunctions::Log<LogTypes::Error>("Failed to create global session.\n");

                    return -1;
                }

                auto &thread_local_diagnostic_info =
                    thread_global_shader_compilation_diagnostic_info[ThreadIndex];
                Slang::ComPtr<slang::IBlob> diagnostic_blob;

                for (auto &Path : ShaderFilePathsToCheck)
                {
                    auto *module =
                        session->loadModule(Path.string().c_str(), diagnostic_blob.writeRef());

                    if (!module)
                    {
                        if (diagnostic_blob)
                        {
                            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                                thread_local_diagnostic_info,
                                "While compiling {}, ran into error: {}\n",
                                Path.string(),
                                diagnostic_blob->getBufferPointer());
                        }

                        continue;
                    }

                    check_module_dependencies(module);
                }
            });

        TaskScheduler.AddTaskSetToPipe(&task);
        TaskScheduler.WaitforTask(&task);

        return 0;
    };

    template <bool Verbose = false> int BuildShaders()
    {
        auto temporary_directory = HelperFunctions::GetInstallationDirectory() / "temp";
        if (!filesystem::exists(temporary_directory))
        {
            HelperFunctions::Log<LogTypes::Info, true, Verbose>(
                "Failed to find temporary directory. Creating a new one...\n");
            std::error_code error_code;
            filesystem::create_directories(temporary_directory, error_code);

            if (error_code)
            {
                HelperFunctions::Log<LogTypes::Info>(
                    "Failed to create temporary directory with error: {}\n", error_code.message());
                return -1;
            }
        }

        if (filesystem::exists(".jslang"))
        {
            std::error_code error_code;
            filesystem::create_directory(".jslang", error_code);
            HelperFunctions::Log<LogTypes::Error>(
                "Failed to create .jslang directory with error: {}\n", error_code.message());
            return -1;
        }

        HelperFunctions::Log<LogTypes::Info, true, Verbose>("Initializing TaskScheduler...\n");

        if (ObjectCLIOptions.ThreadCount == 0)
        {
            ObjectCLIOptions.ThreadCount = std::thread::hardware_concurrency();
        }

        HelperFunctions::Log<LogTypes::Info, true, Verbose>(
            "Building with {} thread(s).\n", ObjectCLIOptions.ThreadCount);

        TaskScheduler.Initialize(ObjectCLIOptions.ThreadCount);

        HelperFunctions::Log<LogTypes::Info, true, Verbose>("TaskScheduler initialized.\n");

        filesystem::path current_working_directory = filesystem::current_path();
        filesystem::path expected_file             = "jslang.toml";

        auto path_to_expected_file = current_working_directory / expected_file;

        if (!filesystem::exists(path_to_expected_file))
        {
            using namespace TerminalTextStyling;

            HelperFunctions::Log<LogTypes::Error>(
                "{}jslang.toml{} not found. Checked for path: {}\n",
                Style::UNDERLINE,
                Style::UNDERLINE_OFF,
                path_to_expected_file.string());

            return 0;
        }

        auto parsed_options = Parser::ParseTOMLFile(expected_file);

        if (!parsed_options)
        {
            return -1;
        };

        HelperFunctions::Log<LogTypes::Info, true, Verbose>("Searching for Slang shaders...\n");

        if (parsed_options->SearchFolders.empty())
        {
            HelperFunctions::Log<LogTypes::Error>(
                "No search path provided ([Build] SearchFolders). Exiting to avoid file "
                "corruption.");
        }

        std::vector<filesystem::path> path_to_shader_files;
        for (const auto &folder_path : parsed_options->SearchFolders)
        {
            HelperFunctions::Log<LogTypes::Info, true, Verbose>(
                "Searching for shaders in directory: {}\n", folder_path);

            std::ranges::move(
                FileGlobber::MultithreadedFileGlobber<false>(
                    TaskScheduler, folder_path, {".slang"}),
                std::back_inserter(path_to_shader_files));

            if constexpr (!Verbose)
            {
                continue;
            }

            for (auto &PathToSlangShader : path_to_shader_files)
            {
                HelperFunctions::Log<LogTypes::Info, true, Verbose>(
                    "Found shader file: {}\n", PathToSlangShader.string());
            }
        }

        if (path_to_shader_files.size() == 0)
        {
            HelperFunctions::Log<LogTypes::Error>(
                "Failed to find shader files. Exiting to avoid corruption.\n");
            return -1;
        }

        HelperFunctions::Log<LogTypes::Success, true, Verbose>(
            "Successfully found shaders ({} of them) and loaded their file paths onto memory. "
            "Proceeding with "
            "next step...\n",
            path_to_shader_files.size());

        if (parsed_options->DoFileContentIntegrityChecks)
        {
            HelperFunctions::Log<LogTypes::Warn>(
                "You have DoFileContentIntegrityChecks set to true. That feature is currently not "
                "implemented. You can disable this warning by setting it to false, or commenting "
                "it out.\n");
        }

        HelperFunctions::Log<LogTypes::Info, true, Verbose>(
            "Attempting to load companion dynamic libraries...\n");

        auto dylib_file_extension = HelperFunctions::GetDLLExtensionForTargetPlatform();

        DynamicLibraryLoader::DynamicLibraryMap EmbeddedDynamicLibraries = {
            {"slang-compiler" + dylib_file_extension, std::span{DYNAMIC_LIBRARY_SLANG_COMPILER}},
            {"slang-glsl" + dylib_file_extension, std::span{DYNAMIC_LIBRARY_SLANG_GLSL_MODULE}},
            {"slang-glslang" + dylib_file_extension, std::span{DYNAMIC_LIBRARY_SLANG_GL_SLANG}},
            {"slang-llvm" + dylib_file_extension, std::span{DYNAMIC_LIBRARY_SLANG_LLVM}},
            {"slang-rt" + dylib_file_extension, std::span{DYNAMIC_LIBRARY_SLANG_RUN_TIME}}};

        auto dylib_caching_result = DynamicLibraryLoader::CacheCompanionDynamicLibraries<Verbose>(
            temporary_directory, EmbeddedDynamicLibraries);

        if (!dylib_caching_result)
        {
            HelperFunctions::Log<LogTypes::Error>("Failed to cache dynamic library.\n");
            return -1;
        }

        auto slang_compiler_dylib =
            DynamicLibraryLoader::LoadDynamicLibrary(temporary_directory / "slang-compiler");

        if (!slang_compiler_dylib)
        {
            return -1;
        }

        HelperFunctions::Log<LogTypes::Success, true, Verbose>(
            "Loaded companion dynamic libraries.\n");

        HelperFunctions::Log<LogTypes::Info, true, Verbose>(
            "Attempting to load the Slang compiler...\n");

        typedef SlangResult (*SlangCreateGlobalSessionFunc)( // NOLINT
            SlangInt ApiVersion, slang::IGlobalSession **OutGlobalSession);

        auto CreateSlangGlobalSessionFunction =
            slang_compiler_dylib->get_function<SlangResult(SlangInt, slang::IGlobalSession **)>(
                "slang_createGlobalSession");

        if (!CreateSlangGlobalSessionFunction)
        {
            HelperFunctions::Log<LogTypes::Error>("Failed to get slang_createGlobalSession.\n");
            return -1;
        }

        Slang::ComPtr<slang::IGlobalSession> GlobalSession;
        SlangResult                          slang_global_session_creation_result =
            CreateSlangGlobalSessionFunction(SLANG_API_VERSION, GlobalSession.writeRef());

        if (SLANG_FAILED(slang_global_session_creation_result))
        {
            HelperFunctions::Log<LogTypes::Error>("Failed to create Slang global session.\n");
        }

        HelperFunctions::Log<LogTypes::Success, true, Verbose>(
            "Successfully loaded Slang compiler onto memory. Proceeding to compile Slang "
            "shaders...\n");

        _compile_shaders(
            TaskScheduler,
            path_to_shader_files,
            GlobalSession,
            parsed_options->TargetDescriptions,
            parsed_options->OutputFolder);

        return 0;
    };
};
} // namespace JSlang

int main(int ArgumentCount, char **ArgumentVector)
{
    CLI::App app{"A simple build system for Slang."};
    app.require_subcommand(1);

    bool verbose = false;
    app.add_flag("-v, --verbose", verbose);

    JSlang::Build ObjectBuild;
    ObjectBuild.TemporaryCacheDirectory =
        JSlang::HelperFunctions::GetInstallationDirectory() / "tmp";
    ObjectBuild.ObjectCLIOptions.Verbose = verbose;
    ObjectBuild.CreateSubCommand(app);

    CLI11_PARSE(app, ArgumentCount, ArgumentVector);

    return 0;
}
