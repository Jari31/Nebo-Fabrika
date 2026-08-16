#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
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

    typedef SlangResult (*SlangCreateGlobalSessionFunc)( // NOLINT
        SlangInt ApiVersion, slang::IGlobalSession **OutGlobalSession);

    using FuncSignatureCreateGlobalSession = SlangResult(SlangInt, slang::IGlobalSession **);
    FuncSignatureCreateGlobalSession *SlangCreateGlobalSession = nullptr;

    using FuncSignatureCreateBlob            = SlangResult(const void *, size_t, slang::IBlob **);
    FuncSignatureCreateBlob *SlangCreateBlob = nullptr;

    enum class Error : uint8_t
    {
        FailedToFindDirectory,
    };

    struct CLIOptions
    {
        bool     Verbose     = false;
        uint32_t ThreadCount = 0;
    } ObjectCLIOptions;

    struct ThreadLocalBuildContext
    {
        Slang::ComPtr<slang::IGlobalSession> GlobalSession;
        Slang::ComPtr<slang::ISession>       Session;
        HashMap::HashMap                    *ThreadLocalHashMap = nullptr;
        HashMap::HashMap                    *ThreadGlobalHashMap =
            nullptr; // WARN: unsafe to write to; safe to read from
    };

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

    static std::string get_formatted_ir_file_path(const filesystem::path &IRFileName)
    {
        return std::format(".jslang/{}.slang-module", IRFileName.stem().string());
    };

    static bool save_ir_blob_file(
        slang::IBlob           *IRBlob,
        const filesystem::path &FileName,
        std::string            &PrintBuffer)
    {
        std::ofstream ir_file(get_formatted_ir_file_path(FileName), std::ios::binary);

        if (!ir_file)
        {
            std::error_code error_code(errno, std::generic_category());
            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                PrintBuffer,
                "Failed to create a slang module IR blob file for {}. Error: {}\n",
                FileName.string(),
                error_code.message());
            return false;
        }

        ir_file.write(
            reinterpret_cast<const char *>(IRBlob->getBufferPointer()),
            IRBlob->getBufferSize()); // NOLINT

        if (!ir_file)
        {
            std::error_code error_code(errno, std::generic_category());
            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                PrintBuffer,
                "Failed to write IR blob data to file {}. Error: {}\n",
                FileName.string(),
                error_code.message());
            return false;
        }

        ir_file.close();

        return true;
    }

    bool load_ir_file_into_session(
        slang::ISession        *Session,
        const filesystem::path &FileName,
        std::string            &PrintBuffer) const
    {
        std::ifstream ir_file(get_formatted_ir_file_path(FileName), std::ios::binary);

        if (!ir_file)
        {
            std::error_code error_code(errno, std::generic_category());
            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                PrintBuffer,
                "Failed to open IR blob file {} for reading. Error: {}\n",
                FileName.string(),
                error_code.message());
            return false;
        }

        uint64_t ir_file_size = ir_file.tellg();
        ir_file.seekg(0, std::ios::beg);

        std::vector<char> ir_blob_data(ir_file_size);
        ir_file.read(ir_blob_data.data(), ir_file_size); // NOLINT

        if (!ir_file)
        {
            std::error_code error_code(errno, std::generic_category());
            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                PrintBuffer,
                "Failed to read IR blob data from file {}. Error: {}\n",
                FileName.string(),
                error_code.message());
            return false;
        }

        ir_file.close();

        Slang::ComPtr<slang::IBlob> ir_blob;
        SlangResult                 ir_blob_creation_result =
            SlangCreateBlob(ir_blob_data.data(), ir_blob_data.size(), ir_blob.writeRef());

        if (SLANG_FAILED(ir_blob_creation_result) || (ir_blob == nullptr))
        {
            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                PrintBuffer, "Failed to create IR blob for module {}.", FileName.stem().string());
            return false;
        }

        Slang::ComPtr<slang::IBlob> diagnostic_blob;

        auto *ir_module = Session->loadModuleFromIRBlob(
            FileName.stem().string().c_str(),
            FileName.string().c_str(),
            ir_blob,
            diagnostic_blob.writeRef());

        if (!ir_module) // NOLINT
        {
            if (diagnostic_blob) // NOLINT
            {
                HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                    PrintBuffer, "Failed to load module {} from IR blob.\n", FileName.string());
            }
            return false;
        }

        return true;
    }

    bool compile_ir_module(
        ThreadLocalBuildContext &Context,
        const char              *DependencyFilePath,
        std::string             &PrintBuffer)
    {
        Slang::ComPtr<slang::IBlob> diagnostic_blob;
        auto *module = Context.Session->loadModule(DependencyFilePath, diagnostic_blob.writeRef());

        auto filesystem_dependency_path = filesystem::path(DependencyFilePath);

        if (!module) // NOLINT
        {
            if (diagnostic_blob) // NOLINT
            {
                HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                    PrintBuffer,
                    "While compiling for module {}, ran into error: {}\n",
                    DependencyFilePath,
                    diagnostic_blob->getBufferPointer());
            }

            return false;
        }

        compile_module_dependencies(Context, module, PrintBuffer);

        Slang::ComPtr<slang::IBlob> ir_blob;
        module->serialize(ir_blob.writeRef());

        if (!save_ir_blob_file(ir_blob, filesystem_dependency_path.filename(), PrintBuffer))
        {
            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                PrintBuffer,
                "Failed to save IR file for module: {}\n",
                filesystem_dependency_path.string());
        }

        auto file_path_hash = HashMap::HashString(filesystem_dependency_path.string());

        auto file_last_edit_time =
            HelperFunctions::GetFileLastWriteTimeAsType<uint64_t>(filesystem_dependency_path);

        Context.ThreadLocalHashMap->insert_or_assign(file_path_hash, file_last_edit_time);
        return true;
    }

    bool compile_module_dependencies(
        ThreadLocalBuildContext &Context,
        slang::IModule          *Module,
        std::string             &PrintBuffer)
    {
        if (Module == nullptr)
        {
            return false;
        }

        auto dependency_count = Module->getDependencyFileCount();

        if (dependency_count == 0)
        {
            return true;
        }

        bool compiled_all_dependencies = false;

        for (SlangInt32 i = 0; i < dependency_count; i++)
        {
            const auto *dependency_file_path = Module->getDependencyFilePath(i);

            auto compile_module_to_ir = [&]() -> void
            {
                compiled_all_dependencies =
                    compile_ir_module(Context, dependency_file_path, PrintBuffer);
                return;
            };

            HelperFunctions::Log<LogTypes::Error>("Got here!\n");

            if (!dependency_file_path) // NOLINT
            {
                HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                    PrintBuffer, "Failed to get dependency file for module.\n");

                return false;
            }

            while (true)
            {
                if (!filesystem::exists(get_formatted_ir_file_path(
                        filesystem::path(dependency_file_path).filename())))
                {
                    compile_module_to_ir();
                    break;
                }

                if (Context.ThreadGlobalHashMap->empty())
                {
                    compile_module_to_ir();
                    break;
                }

                auto hash_map_key = HashMap::HashString(dependency_file_path);
                if (!Context.ThreadGlobalHashMap->contains(hash_map_key))
                {
                    compile_module_to_ir();
                    break;
                }

                auto file_last_edit_time =
                    HelperFunctions::GetFileLastWriteTimeAsType<uint64_t>(dependency_file_path);
                if (file_last_edit_time != Context.ThreadGlobalHashMap->at(hash_map_key))
                {
                    compile_module_to_ir();
                    break;
                }

                if (!filesystem::exists(get_formatted_ir_file_path(
                        filesystem::path(dependency_file_path).filename())))
                {
                    compile_module_to_ir();
                };

                break;
            }

            if (compiled_all_dependencies)
            {
                continue;
            }

            if (!load_ir_file_into_session(
                    Context.Session,
                    filesystem::path(dependency_file_path).filename(),
                    PrintBuffer))
            {
                compile_module_to_ir();
            }

            if (!compiled_all_dependencies)
            {
                return false;
            }
        }

        return compiled_all_dependencies;
    }

    int compile_shaders(
        enki::TaskScheduler                                   &TaskScheduler,
        std::vector<std::string>                              &SearchPaths,
        std::vector<filesystem::path>                         &ShaderFilePathsToCheck,
        std::vector<Parser::ParsedOptions::TargetDescription> &TargetDescriptions,
        const filesystem::path                                &OutputFolder)
    {
        HashMap::HashMap dependency_hash_map;

        if (filesystem::exists(".jslang/build_manifest.bin"))
        {
            dependency_hash_map = HashMap::LoadHashMapFromDisk(".jslang/dependency_hash_map.bin");
        }

        auto thread_count = TaskScheduler.GetNumTaskThreads();

        std::vector<std::string> thread_global_print_buffer;
        thread_global_print_buffer.resize(thread_count);

        std::vector<HashMap::HashMap> thread_global_hash_map_buffer;
        thread_global_hash_map_buffer.resize(thread_count);

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
                auto &thread_local_print_buffer    = thread_global_print_buffer[ThreadIndex];
                auto &thread_local_hash_map_buffer = thread_global_hash_map_buffer[ThreadIndex];

                ThreadLocalBuildContext context;
                context.ThreadGlobalHashMap = &dependency_hash_map;
                context.ThreadLocalHashMap  = &thread_local_hash_map_buffer;

                SlangResult slang_global_session_creation_result =
                    SlangCreateGlobalSession(SLANG_API_VERSION, context.GlobalSession.writeRef());

                if (SLANG_FAILED(slang_global_session_creation_result))
                {
                    HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                        thread_local_print_buffer, "Failed to create Slang global session.\n");
                    return;
                }
                slang::SessionDesc session_description = {};

                std::vector<slang::TargetDesc> target_descriptions;
                target_descriptions.reserve(TargetDescriptions.size());

                for (auto &TargetDescription : TargetDescriptions)
                {
                    slang::TargetDesc target_description = {};
                    target_description.format = parse_target_format(TargetDescription.Format);
                    target_description.profile =
                        context.GlobalSession->findProfile(TargetDescription.Profile.c_str());

                    if (TargetDescription.GenerateSPIRVDirectly)
                    {
                        target_description.flags |= SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
                    }
                    if (TargetDescription.GenerateWholeProgram)
                    {
                        target_description.flags |= SLANG_TARGET_FLAG_GENERATE_WHOLE_PROGRAM;
                    }

                    target_descriptions.push_back(target_description);
                }

                std::vector<const char *> search_path_pointers;
                search_path_pointers.reserve(SearchPaths.size());
                for (const auto &path : SearchPaths)
                {
                    search_path_pointers.push_back(path.c_str());
                }

                session_description.searchPaths = search_path_pointers.data();
                session_description.searchPathCount =
                    static_cast<SlangInt>(search_path_pointers.size());

                session_description.targets     = target_descriptions.data();
                session_description.targetCount = static_cast<SlangInt>(target_descriptions.size());

                if (SLANG_FAILED(context.GlobalSession->createSession(
                        session_description, context.Session.writeRef())))
                {
                    HelperFunctions::Log<LogTypes::Error>("Failed to create local session.\n");

                    return;
                }

                Slang::ComPtr<slang::IBlob> diagnostic_blob;

                for (uint32_t i = Range.start; i < Range.end; i++)
                {
                    auto &path   = ShaderFilePathsToCheck[i];
                    auto *module = context.Session->loadModule(
                        path.string().c_str(), diagnostic_blob.writeRef());

                    if (!module || diagnostic_blob)
                    {
                        if (diagnostic_blob)
                        {
                            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                                thread_local_print_buffer,
                                "While loading module {}, ran into error: {}\n",
                                path.string(),
                                static_cast<const char *>(diagnostic_blob->getBufferPointer()));
                        }

                        continue;
                    }

                    auto *module_layout = module->getLayout();

                    if (!module_layout || module_layout->getEntryPointCount() == 0)
                    {
                        HelperFunctions::Log<LogTypes::Error>(
                            "Got here! Thread calling this: {}\n", ThreadIndex);
                        continue;
                    }

                    if (!compile_module_dependencies(context, module, thread_local_print_buffer))
                    {
                        HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                            thread_local_print_buffer,
                            "Failed to compile dependency modules for {}.\n",
                            path.string());
                    }
                }
            });

        TaskScheduler.AddTaskSetToPipe(&task);
        TaskScheduler.WaitforTask(&task);

        for (auto &PrintBuffer : thread_global_print_buffer)
        {
            if (PrintBuffer.size() > 0)
            {
                std::cout << PrintBuffer << " | " << PrintBuffer.size() << "\n";
            }
        }

        size_t total_hash_map_size = dependency_hash_map.size();
        for (const auto &HashMap : thread_global_hash_map_buffer)
        {
            total_hash_map_size += HashMap.size();
        }
        dependency_hash_map.reserve(total_hash_map_size);

        for (auto &HashMap : thread_global_hash_map_buffer)
        {

            for (auto &&[Key, Value] : HashMap)
            {
                dependency_hash_map.insert_or_assign(Key, Value);
            }
        }

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

        if (!filesystem::exists(".jslang"))
        {
            std::error_code error_code;
            filesystem::create_directory(".jslang", error_code);

            if (error_code)
            {
                HelperFunctions::Log<LogTypes::Error>(
                    "Failed to create .jslang directory with error: {}\n", error_code.message());
                return -1;
            }
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
                FileGlobber::MultithreadedFileGlobber(TaskScheduler, folder_path, {".slang"}),
                std::back_inserter(path_to_shader_files));
        }

        if constexpr (Verbose)
        {
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

        SlangCreateGlobalSession =
            slang_compiler_dylib->get_function<SlangResult(SlangInt, slang::IGlobalSession **)>(
                "slang_createGlobalSession");

        if (!SlangCreateGlobalSession)
        {
            HelperFunctions::Log<LogTypes::Error>(
                "Failed to get slang_createGlobalSession. (Maybe the slang-compiler dynamic "
                "library doesn't exist?)\n");
            return -1;
        }

        SlangCreateBlob =
            slang_compiler_dylib->get_function<FuncSignatureCreateBlob>("slang_createBlob");

        HelperFunctions::Log<LogTypes::Success, true, Verbose>(
            "Successfully loaded Slang compiler onto memory. Proceeding to compile Slang "
            "shaders...\n");

        if (compile_shaders(
                TaskScheduler,
                parsed_options->SearchFolders,
                path_to_shader_files,
                parsed_options->TargetDescriptions,
                parsed_options->OutputFolder) != 0)
        {
            HelperFunctions::Log<LogTypes::Error>("Failed to compile shaders.\n");
        }

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
