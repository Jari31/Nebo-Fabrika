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

    using FuncSignatureCreateBlob            = slang::IBlob *(const void *, size_t);
    FuncSignatureCreateBlob *SlangCreateBlob = nullptr;

    enum class Error : uint8_t
    {
        FailedToFindDirectory,
    };

    struct CLIOptions
    {
        bool     Verbose        = false;
        bool     DoCleanRebuild = false;
        uint32_t ThreadCount    = 0;
    } ObjectCLIOptions;

    struct ThreadLocalBuildContext
    {
        using TypeIRBlobStorage =
            ankerl::unordered_dense::map<filesystem::path, std::vector<uint8_t>>;

        Slang::ComPtr<slang::IGlobalSession> GlobalSession;
        Slang::ComPtr<slang::ISession>       Session;
        HashMap::HashMap                    *ThreadLocalHashMap = nullptr;
        HashMap::HashMap                    *ThreadGlobalHashMap =
            nullptr; // WARN: unsafe to write to; safe to read from

        TypeIRBlobStorage                   *ThreadLocalIRBlobBuffer = nullptr;
        std::vector<slang::IComponentType *> ShaderModules;
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
        subcommand->add_flag(
            "-c, --clean",
            ObjectCLIOptions.DoCleanRebuild,
            "Whether to delete cache files and do a clean rebuild.");

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

    /// WARN: Not thread safe.
    template <bool Verbose = false>
    static bool
    save_ir_blob_file(const std::vector<uint8_t> &IRSource, const filesystem::path &FileName)
    {
        HelperFunctions::Log<LogTypes::Info, true, Verbose>(
            "Saving IR file {}...\n", FileName.string());

        if (IRSource.empty())
        {
            HelperFunctions::Log<LogTypes::Error>(
                "IR source is empty. Exiting to avoid corruption.\n");
            return false;
        }

        auto ir_file_save_path = get_formatted_ir_file_path(FileName);

        std::ofstream ir_file(ir_file_save_path + ".temp", std::ios::binary);

        if (!ir_file)
        {
            std::error_code error_code(errno, std::generic_category());
            HelperFunctions::Log<LogTypes::Error>(
                "Failed to create a slang module IR blob file for {}. Error: {}\n",
                FileName.string(),
                error_code.message());
            return false;
        }

        ir_file.write(reinterpret_cast<const char *>(IRSource.data()),
                      IRSource.size()); // NOLINT

        if (!ir_file)
        {
            std::error_code error_code(errno, std::generic_category());
            HelperFunctions::Log<LogTypes::Error>(
                "Failed to write IR blob data to file {}. Error: {}\n",
                FileName.string(),
                error_code.message());
            return false;
        }

        ir_file.close();

        if (ir_file.good())
        {
            std::error_code error_code;
            std::filesystem::rename(ir_file_save_path + ".temp", ir_file_save_path, error_code);

            if (error_code)
            {
                HelperFunctions::Log<LogTypes::Error>(
                    "Failed to rename for IR file {}: {}", ir_file_save_path, error_code.message());
                return false;
            }
        }
        else
        {
            HelperFunctions::Log<LogTypes::Error>(
                "Failed to save IR file for {}\n. (Maybe there isn't enough space? Maybe the "
                "program doesn't have proper permissions?)",
                FileName.string());
            return false;
        }

        HelperFunctions::Log<LogTypes::Success, true, Verbose>("Successfully saved IR file.\n");

        return true;
    }
    template <bool Verbose = false>
    bool load_ir_file_into_session(
        ThreadLocalBuildContext &Context,
        const filesystem::path  &FileName,
        std::string             &PrintBuffer) const
    {
        const auto ir_file_name = get_formatted_ir_file_path(FileName);

        HelperFunctions::ThreadSafeLog<LogTypes::Info, true, Verbose>(
            PrintBuffer, "Loading IR file {}...\n", ir_file_name);

        std::ifstream ir_file(ir_file_name, std::ios::binary | std::ios::ate);

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
        ir_blob = SlangCreateBlob(ir_blob_data.data(), ir_blob_data.size());

        if (ir_blob == nullptr)
        {
            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                PrintBuffer, "Failed to create IR blob for module {}.\n", FileName.stem().string());
            return false;
        }

        Slang::ComPtr<slang::IBlob> diagnostic_blob;

        auto *module = Context.Session->loadModuleFromIRBlob(
            FileName.stem().string().c_str(),
            FileName.string().c_str(),
            ir_blob,
            diagnostic_blob.writeRef());

        if (!module || diagnostic_blob) // NOLINT
        {
            if (diagnostic_blob) // NOLINT
            {
                HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                    PrintBuffer, "Failed to load module {} from IR blob.\n", FileName.string());
            }
            else
            {
                HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                    PrintBuffer, "Failed to load module {}.\n", FileName.stem().string());
            }
            return false;
        }

        Context.ShaderModules.push_back(module);

        HelperFunctions::ThreadSafeLog<LogTypes::Success, true, Verbose>(
            PrintBuffer, "Successfully loaded IR file.\n");
        return true;
    }

    template <bool Verbose = false>
    static bool compile_ir_module(
        ThreadLocalBuildContext &Context,
        const char              *DependencyFilePath,
        std::string             &PrintBuffer)
    {
        HelperFunctions::ThreadSafeLog<LogTypes::Info, true, Verbose>(
            PrintBuffer, "Compiling dependency: {}\n", DependencyFilePath);

        Slang::ComPtr<slang::IBlob> diagnostic_blob;
        auto *module = Context.Session->loadModule(DependencyFilePath, diagnostic_blob.writeRef());

        auto filesystem_dependency_path = filesystem::path(DependencyFilePath);

        if (!module || diagnostic_blob) // NOLINT
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

        Slang::ComPtr<slang::IBlob> ir_blob;
        module->serialize(ir_blob.writeRef());
        if (ir_blob == nullptr)
        {
            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                PrintBuffer, "Failed to serialize IR blob.\n");
            return false;
        }

        const auto *ir_blob_source = static_cast<const uint8_t *>(ir_blob->getBufferPointer());
        size_t      ir_blob_size   = ir_blob->getBufferSize();

        auto ir_blob_buffer = std::vector<uint8_t>(ir_blob_source, ir_blob_source + ir_blob_size);

        auto file_path_hash = HashMap::HashString(filesystem_dependency_path.string());

        auto file_last_edit_time =
            HelperFunctions::GetFileLastWriteTimeAsType<uint64_t>(filesystem_dependency_path);

        Context.ThreadLocalHashMap->try_emplace(
            file_path_hash,
            file_last_edit_time); // WARN: Might get corrupted because it doesn't track the IR
                                  // WARN: file's properties. Most likely won't
        Context.ThreadLocalIRBlobBuffer->try_emplace(
            filesystem_dependency_path, std::move(ir_blob_buffer));

        HelperFunctions::ThreadSafeLog<LogTypes::Success, true, Verbose>(
            PrintBuffer, "Successfully compiled dependency module into an IR format.\n");

        Context.ShaderModules.push_back(module);
        return true;
    }

    enum class DependencyCompilationResult : uint8_t
    {
        FailedToCompileAndLoadModules,
        CompiledAndLoadedAllModules,
        LoadedAllModules,
    };

    template <bool Verbose = false>
    DependencyCompilationResult compile_module_dependencies(
        ThreadLocalBuildContext &Context,
        slang::IModule          *Module,
        std::string             &PrintBuffer) const
    {
        if (Module == nullptr)
        {
            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                PrintBuffer, "Provided module for dependency resolution is null; unusable.\n");
            return DependencyCompilationResult::FailedToCompileAndLoadModules;
        }

        auto dependency_count = Module->getDependencyFileCount();

        if (dependency_count == 0)
        {
            return DependencyCompilationResult::CompiledAndLoadedAllModules;
        }

        Context.ShaderModules.reserve(dependency_count);
        Context.ShaderModules.push_back(Module);

        HelperFunctions::ThreadSafeLog<LogTypes::Info, true, Verbose>(
            PrintBuffer,
            "Compiling for {} dependencies for module {}.\n",
            dependency_count,
            Module->getName());

        bool loaded_all_modules = true; // loaded means the main file may not need to be compiled
                                        // since dependencies stay the same

        for (SlangInt32 i = dependency_count - 1; i > 0;
             i--) // dependency 0 is the base shader (i.e., the root file in the DAG that's
                  // importing all this)
        {
            bool        compiled_all_dependencies = false;
            const auto *dependency_file_path      = Module->getDependencyFilePath(i);

            auto compile_module_to_ir = [&]() -> void
            {
                compiled_all_dependencies =
                    compile_ir_module<Verbose>(Context, dependency_file_path, PrintBuffer);
                if (!compiled_all_dependencies)
                {
                    HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                        PrintBuffer,
                        "Failed to compile dependency module {}\n",
                        dependency_file_path);
                }

                loaded_all_modules = false;
                return;
            };

            if (!dependency_file_path) // NOLINT
            {
                HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                    PrintBuffer, "Failed to get dependency file for module.\n");

                return DependencyCompilationResult::FailedToCompileAndLoadModules;
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
                }
                break;
            }

            if (compiled_all_dependencies)
            {
                continue;
            }

            if (!load_ir_file_into_session<Verbose>(
                    Context, filesystem::path(dependency_file_path).filename(), PrintBuffer))
            {
                compile_module_to_ir();
            }
            else
            {
                compiled_all_dependencies = true;
            }

            if (!compiled_all_dependencies)
            {
                return DependencyCompilationResult::FailedToCompileAndLoadModules;
            }
        }
        if (loaded_all_modules)
        {
            return DependencyCompilationResult::LoadedAllModules;
        }
        return DependencyCompilationResult::CompiledAndLoadedAllModules;
    }
    template <bool Verbose = false>
    int compile_shaders(
        enki::TaskScheduler                                   &TaskScheduler,
        std::vector<std::string>                              &SearchPaths,
        std::vector<filesystem::path>                         &ShaderFilePathsToCheck,
        std::vector<Parser::ParsedOptions::TargetDescription> &TargetDescriptions,
        const filesystem::path                                &OutputFolder) const
    {
        HashMap::HashMap                           dependency_hash_map;
        ThreadLocalBuildContext::TypeIRBlobStorage dependency_ir_blobs;

        if (filesystem::exists(".jslang/dependency_hash_map.bin"))
        {
            dependency_hash_map = HashMap::LoadHashMapFromDisk(".jslang/dependency_hash_map.bin");
        }

        auto thread_count = TaskScheduler.GetNumTaskThreads();

        std::vector<std::string> thread_global_print_buffer;
        thread_global_print_buffer.resize(thread_count);

        std::vector<HashMap::HashMap> thread_global_hash_map_buffer;
        thread_global_hash_map_buffer.resize(thread_count);

        std::vector<ThreadLocalBuildContext::TypeIRBlobStorage> thread_global_ir_blob_buffer;
        thread_global_ir_blob_buffer.resize(thread_count);

        auto parse_target_format = [&](std::string &Format,
                                       std::string &PrintBuffer) -> SlangCompileTarget
        {
            std::ranges::transform(Format, Format.begin(), ::tolower);

            static const ankerl::unordered_dense::map<std::string_view, SlangCompileTarget>
                extension_to_target_map = {
                    {".glsl", SLANG_GLSL},
                    {".hlsl", SLANG_HLSL},
                    {".spv", SLANG_SPIRV},
                    {".spvasm", SLANG_SPIRV_ASM},
                    {".dxbc", SLANG_DXBC},
                    {".dxbcasm", SLANG_DXBC_ASM},
                    {".dxil", SLANG_DXIL},
                    {".dxilasm", SLANG_DXIL_ASM},
                    {".c", SLANG_C_SOURCE},
                    {".cpp", SLANG_CPP_SOURCE},
                    {".exe", SLANG_HOST_EXECUTABLE},
                    {".cu", SLANG_CUDA_SOURCE},
                    {".ptx", SLANG_PTX},
                    {".metal", SLANG_METAL},
                    {".metallib", SLANG_METAL_LIB},
                    {".metallibasm", SLANG_METAL_LIB_ASM},
                    {".wgsl", SLANG_WGSL},
                    {".slangvm", SLANG_HOST_VM},
                    {".h", SLANG_CPP_HEADER},
                    {".cuh", SLANG_CUDA_HEADER},
                    {".ll", SLANG_SHADER_LLVM_IR},
                    {".dll", SLANG_SHADER_SHARED_LIBRARY},
                    {".o", SLANG_OBJECT_CODE}};

            auto iterator = extension_to_target_map.find(Format);
            if (iterator != extension_to_target_map.end())
            {
                return iterator->second;
            }

            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                PrintBuffer, "Unknown format \"{}.\"\n", Format);

            return SLANG_TARGET_UNKNOWN;
        };

        std::vector<std::vector<filesystem::path>> thread_global_shader_compilation_list(
            thread_count);
        enki::TaskSet task(
            ShaderFilePathsToCheck.size(),
            [&](enki::TaskSetPartition Range, uint32_t ThreadIndex) -> void
            {
                auto &thread_local_print_buffer = thread_global_print_buffer[ThreadIndex];

                ThreadLocalBuildContext context;
                context.ThreadGlobalHashMap     = &dependency_hash_map;
                context.ThreadLocalHashMap      = &thread_global_hash_map_buffer[ThreadIndex];
                context.ThreadLocalIRBlobBuffer = &thread_global_ir_blob_buffer[ThreadIndex];

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
                    auto target_format =
                        parse_target_format(TargetDescription.Format, thread_local_print_buffer);

                    if (target_format == SLANG_TARGET_UNKNOWN)
                    {
                        return;
                    }

                    slang::TargetDesc target_description = {};
                    target_description.format            = target_format;
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
                    HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                        thread_local_print_buffer, "Failed to create local session.\n");

                    return;
                }

                Slang::ComPtr<slang::IBlob> diagnostic_blob;

                for (uint32_t i = Range.start; i < Range.end; i++)
                {
                    auto &path_to_module = ShaderFilePathsToCheck[i];
                    auto *module         = context.Session->loadModule(
                        path_to_module.string().c_str(), diagnostic_blob.writeRef());

                    auto module_last_edit_time =
                        HelperFunctions::GetFileLastWriteTimeAsType<uint64_t>(path_to_module);
                    auto module_name_hash = HashMap::HashString(path_to_module.string());

                    auto module_name_string = path_to_module.stem().string();

                    auto build_manifest_file_path =
                        ".jslang/" + module_name_string + ".jslang-build";

                    if (!module || diagnostic_blob)
                    {
                        if (diagnostic_blob)
                        {
                            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                                thread_local_print_buffer,
                                "While loading module {}, ran into error: {}\n",
                                path_to_module.string(),
                                static_cast<const char *>(diagnostic_blob->getBufferPointer()));
                        }

                        continue;
                    }

                    // auto *module_layout = module->getLayout();
                    auto module_entry_point_count = module->getDefinedEntryPointCount();

                    if (module_entry_point_count == 0)
                    {
                        continue;
                    }

                    HelperFunctions::ThreadSafeLog<LogTypes::Info, true, Verbose>(
                        thread_local_print_buffer, "Compiling for module: {}\n", module->getName());

                    auto module_compilation_result = compile_module_dependencies<Verbose>(
                        context, module, thread_local_print_buffer);

                    if (module_compilation_result ==
                        DependencyCompilationResult::FailedToCompileAndLoadModules)
                    {
                        HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                            thread_local_print_buffer,
                            "Failed to compile dependency modules for {}.\n",
                            path_to_module.string());
                    }
                    else if (
                        module_compilation_result == DependencyCompilationResult::LoadedAllModules)
                    {
                        auto module_hash_key = HashMap::HashString(path_to_module.string());

                        if (context.ThreadGlobalHashMap->contains(module_hash_key))
                        {
                            auto build_manifest_file_last_edit_time =
                                HelperFunctions::GetFileLastWriteTimeAsType<uint64_t>(
                                    build_manifest_file_path);

                            if (context.ThreadGlobalHashMap->at(module_hash_key) ==
                                (module_last_edit_time ^ build_manifest_file_last_edit_time))
                            {
                                HelperFunctions::ThreadSafeLog<LogTypes::Info, true, Verbose>(
                                    thread_local_print_buffer,
                                    "Cache hit for module {}. Skipping compilation.\n",
                                    module_name_string);
                                continue;
                            }
                        }
                    }

                    HelperFunctions::ThreadSafeLog<LogTypes::Info, true, Verbose>(
                        thread_local_print_buffer,
                        "Compiling for {} entry point(s) in {}.\n",
                        module_entry_point_count,
                        module_name_string);

                    bool compiled_all_entry_points =
                        true; // returning here will leave out other shaders from compilation, which
                              // is unwanted

                    // uint32_t shader_modules_size = context.ShaderModules.size();
                    context.ShaderModules.reserve(module_entry_point_count);
                    for (SlangInt32 j = 0; j < module_entry_point_count; j++)
                    {
                        Slang::ComPtr<slang::IEntryPoint> module_entry_point;
                        SlangResult                       result_of_get_module_entry_point =
                            module->getDefinedEntryPoint(j, module_entry_point.writeRef());
                        if (!module_entry_point || SLANG_FAILED(result_of_get_module_entry_point))
                        {
                            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                                thread_local_print_buffer,
                                "Failed to get entry point at index {} for module {}.\n",
                                i,
                                module_name_string);

                            compiled_all_entry_points = false;
                            continue;
                        }

                        context.ShaderModules.push_back(module_entry_point.get());
                    }
                    Slang::ComPtr<slang::IComponentType> composed_program;

                    SlangResult module_compose_result =
                        context.Session->createCompositeComponentType(
                            context.ShaderModules.data(),
                            static_cast<SlangInt32>(context.ShaderModules.size()),
                            composed_program.writeRef(),
                            diagnostic_blob.writeRef());

                    if (SLANG_FAILED(module_compose_result) || diagnostic_blob)
                    {
                        if (diagnostic_blob)
                        {
                            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                                thread_local_print_buffer,
                                "Failed to compose module with error: {}\n",
                                (const char *)diagnostic_blob->getBufferPointer());
                        }

                        compiled_all_entry_points = false;
                        continue;
                    }

                    Slang::ComPtr<slang::IComponentType> linked_program;
                    SlangResult                          link_result = composed_program->link(
                        linked_program.writeRef(), diagnostic_blob.writeRef());

                    if (SLANG_FAILED(link_result))
                    {
                        // Log diagnostic blob to see if linking/importing failed
                        if (diagnostic_blob)
                        {
                            HelperFunctions::Log(
                                "Link Error: {}\n",
                                (const char *)diagnostic_blob->getBufferPointer());
                        }

                        compiled_all_entry_points = false;
                        continue;
                    }

                    for (SlangInt j = 0; j < module_entry_point_count; j++)
                    {
                        auto *linked_program_layout = linked_program->getLayout();

                        const auto *module_entry_point_name =
                            linked_program_layout->getEntryPointByIndex(j)->getName();

                        SlangInt k = 0; // NOLINT
                        for (auto &TargetDescription : target_descriptions)
                        {
                            auto &target_format                  = TargetDescription.format;
                            auto &target_format_extension_string = TargetDescriptions.at(k).Format;

                            Slang::ComPtr<slang::IBlob> compiled_code_blob;

                            SlangResult target_compilation_result =
                                composed_program->getEntryPointCode(
                                    j,
                                    k,
                                    compiled_code_blob.writeRef(),
                                    diagnostic_blob.writeRef());

                            if (SLANG_FAILED(target_compilation_result) || !compiled_code_blob)
                            {
                                HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                                    thread_local_print_buffer,
                                    "Failed to compile code for target format {}. Error: {}\n",
                                    TargetDescriptions.at(k).Format,
                                    (const char *)diagnostic_blob->getBufferPointer());

                                compiled_all_entry_points = false;
                                continue;
                            }

                            bool is_binary_file = false;
                            switch (target_format)
                            {
                            // INFO:  Text / Source / Assembly Formats
                            case SLANG_GLSL:
                            case SLANG_GLSL_VULKAN_DEPRECATED:
                            case SLANG_GLSL_VULKAN_ONE_DESC_DEPRECATED:
                            case SLANG_HLSL:
                            case SLANG_SPIRV_ASM:
                            case SLANG_DXBC_ASM:
                            case SLANG_DXIL_ASM:
                            case SLANG_C_SOURCE:
                            case SLANG_CPP_SOURCE:
                            case SLANG_CUDA_SOURCE:
                            case SLANG_PTX:
                            case SLANG_HOST_CPP_SOURCE:
                            case SLANG_CPP_PYTORCH_BINDING:
                            case SLANG_METAL:
                            case SLANG_METAL_LIB_ASM:
                            case SLANG_WGSL:
                            case SLANG_WGSL_SPIRV_ASM:
                            case SLANG_CPP_HEADER:
                            case SLANG_CUDA_HEADER:
                            case SLANG_HOST_LLVM_IR:
                            case SLANG_SHADER_LLVM_IR:
                                is_binary_file = false;
                                break;

                            // INFO: Binary / Bytecode / Object Formats
                            case SLANG_TARGET_UNKNOWN:
                            case SLANG_TARGET_NONE:
                            case SLANG_SPIRV:
                            case SLANG_DXBC:
                            case SLANG_DXIL:
                            case SLANG_HOST_EXECUTABLE:
                            case SLANG_SHADER_SHARED_LIBRARY:
                            case SLANG_SHADER_HOST_CALLABLE:
                            case SLANG_CUDA_OBJECT_CODE:
                            case SLANG_OBJECT_CODE:
                            case SLANG_HOST_HOST_CALLABLE:
                            case SLANG_METAL_LIB:
                            case SLANG_HOST_SHARED_LIBRARY:
                            case SLANG_WGSL_SPIRV:
                            case SLANG_HOST_VM:
                            case SLANG_HOST_OBJECT_CODE:
                            case SLANG_TARGET_COUNT_OF:
                            default:
                                is_binary_file = true;
                                break;
                            }

                            auto module_entry_point_name_string =
                                module_name_string + "_" + std::string(module_entry_point_name);

                            filesystem::path output_file_path =
                                ".jslang/" + module_entry_point_name_string + ".temp";

                            std::ios_base::openmode open_mode = std::ios::out;
                            if (is_binary_file)
                            {
                                open_mode |= std::ios::binary;
                            }

                            std::ofstream output_file(output_file_path, open_mode);
                            if (!output_file)
                            {
                                HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                                    thread_local_print_buffer,
                                    "Failed to open file for extension {}, target entry {},"
                                    " module "
                                    "{}.\n",
                                    target_format_extension_string,
                                    k,
                                    module->getName());

                                compiled_all_entry_points = false;
                                continue;
                            }

                            output_file.write(
                                static_cast<const char *>(compiled_code_blob->getBufferPointer()),
                                compiled_code_blob->getBufferSize()); // NOLINT
                            output_file.close();

                            if (output_file.good())
                            {

                                std::error_code error_code;

                                auto true_output_path =
                                    OutputFolder / module_entry_point_name_string.append(
                                                       target_format_extension_string);

                                filesystem::rename(output_file_path, true_output_path, error_code);

                                if (error_code)
                                {
                                    HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                                        thread_local_print_buffer,
                                        "Failed to rename file {} with error: {}\n",
                                        output_file_path.string(),
                                        error_code.message());

                                    compiled_all_entry_points = false;
                                    continue;
                                }

                                HelperFunctions::ThreadSafeLog<LogTypes::Success, true, Verbose>(
                                    thread_local_print_buffer,
                                    "Shader compiled as {}.\n",
                                    true_output_path.string());
                            }
                            else
                            {
                                HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                                    thread_local_print_buffer,
                                    "Failed to write to file {}.\n",
                                    output_file_path.string());

                                compiled_all_entry_points = false;
                                continue;
                            }

                            ++k;
                        }
                    }

                    if (compiled_all_entry_points)
                    {
                        std::ofstream build_manifest_file(build_manifest_file_path);

                        if (!build_manifest_file)
                        {
                            HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                                thread_local_print_buffer,
                                "Failed to open build manifest file for {}.",
                                build_manifest_file_path);

                            continue;
                        }

                        build_manifest_file.close();
                        if (build_manifest_file.good())
                        {
                            auto manifest_file_edit_time =
                                HelperFunctions::GetFileLastWriteTimeAsType<uint64_t>(
                                    build_manifest_file_path);

                            context.ThreadLocalHashMap->insert_or_assign(
                                module_name_hash, module_last_edit_time ^ manifest_file_edit_time);
                            continue;
                        }

                        HelperFunctions::ThreadSafeLog<LogTypes::Error>(
                            thread_local_print_buffer,
                            "Failed to close file for {}.",
                            build_manifest_file_path);
                    }
                }
            });

        TaskScheduler.AddTaskSetToPipe(&task);
        TaskScheduler.WaitforTask(&task);

        for (auto &PrintBuffer : thread_global_print_buffer)
        {
            if (PrintBuffer.size() > 0)
            {
                std::cout << PrintBuffer;
            }
        }

        auto merge_into = [](auto &DestinationBuffer, auto &SourceBuffers, auto &&CallbackLambda)
        {
            size_t total_size_of_destination_buffer = DestinationBuffer.size();
            for (const auto &Buffer : SourceBuffers)
            {
                total_size_of_destination_buffer += Buffer.size();
            }
            DestinationBuffer.reserve(total_size_of_destination_buffer);

            for (const auto &Buffer : SourceBuffers)
            {
                for (auto &&[Key, Value] : Buffer)
                {
                    CallbackLambda(
                        DestinationBuffer,
                        std::forward<decltype(Key)>(Key),
                        std::forward<decltype(Value)>(Value));
                }
            }
        };

        merge_into(
            dependency_ir_blobs,
            thread_global_ir_blob_buffer,
            [](auto &DestinationBuffer, auto &Key, auto &Value)
            { DestinationBuffer.try_emplace(Key, Value); });

        for (auto &&[FilePath, IRSource] : dependency_ir_blobs)
        {
            save_ir_blob_file<Verbose>(IRSource, FilePath.filename());
        }

        merge_into(
            dependency_hash_map,
            thread_global_hash_map_buffer,
            [](auto &DestinationBuffer, auto &Key, auto &Value)
            { DestinationBuffer.insert_or_assign(Key, Value); });

        HashMap::SaveHashMapToDisk(".jslang/dependency_hash_map.bin", dependency_hash_map);

        return 0;
    };

    template <bool Verbose = false>
    bool create_temporary_directories(
        const filesystem::path &TemporaryDynamicLibraryFolder,
        const filesystem::path &TemporaryBuildFolder) const
    {
        if (!filesystem::exists(TemporaryDynamicLibraryFolder))
        {
            HelperFunctions::Log<LogTypes::Info, true, Verbose>(
                "Failed to find temporary directory. Creating a new one...\n");
            std::error_code error_code;
            filesystem::create_directories(TemporaryDynamicLibraryFolder, error_code);

            if (error_code)
            {
                HelperFunctions::Log<LogTypes::Info>(
                    "Failed to create temporary dynamic library folder {} with error: {}\n",
                    TemporaryDynamicLibraryFolder.string(),
                    error_code.message());
                return false;
            }
        }
        else if (ObjectCLIOptions.DoCleanRebuild)
        {
            std::error_code error_code;
            filesystem::remove_all(TemporaryDynamicLibraryFolder, error_code);

            if (error_code)
            {
                HelperFunctions::Log(
                    "Failed to delete temporary dynamic library folder {} with error: {}\n",
                    TemporaryDynamicLibraryFolder.string(),
                    error_code.message());
                return false;
            }
        }

        if (!filesystem::exists(TemporaryBuildFolder))
        {
            std::error_code error_code;
            filesystem::create_directories(TemporaryBuildFolder, error_code);

            if (error_code)
            {
                HelperFunctions::Log<LogTypes::Error>(
                    "Failed to create .jslang directory {} with error: {}\n",
                    TemporaryBuildFolder.string(),
                    error_code.message());
                return false;
            }
        }
        else if (ObjectCLIOptions.DoCleanRebuild)
        {
            std::error_code error_code;
            filesystem::remove_all(TemporaryBuildFolder, error_code);

            if (error_code)
            {
                HelperFunctions::Log(
                    "Failed to delete temporary build folder {} with error: {}\n",
                    TemporaryBuildFolder.string(),
                    error_code.message());
                return false;
            }
        }

        return true;
    };

    template <bool Verbose = false> int BuildShaders()
    {
        auto temporary_dylib_folder = HelperFunctions::GetInstallationDirectory() / "temp";
        auto temporary_build_folder = filesystem::path(".jslang");
        if (!create_temporary_directories<Verbose>(temporary_dylib_folder, temporary_build_folder))
        {
            return -1;
        }

        if (!create_temporary_directories<Verbose>(temporary_dylib_folder, temporary_build_folder))
        {
            return -1;
        };

        HelperFunctions::Log<LogTypes::Info, true, Verbose>("Initializing TaskScheduler...\n");

        if (ObjectCLIOptions.ThreadCount == 0)
        {
            ObjectCLIOptions.ThreadCount = std::thread::hardware_concurrency();
        }

        HelperFunctions::Log<LogTypes::Info, true, Verbose>(
            "Building with {} thread(s).\n", ObjectCLIOptions.ThreadCount);

        TaskScheduler.Initialize(ObjectCLIOptions.ThreadCount);

        HelperFunctions::Log<LogTypes::Success, true, Verbose>("TaskScheduler initialized.\n");

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

        if (parsed_options->SearchFolders.empty() || parsed_options->OutputFolder.empty())
        {
            HelperFunctions::Log<LogTypes::Error>(
                "No search path (or output path) provided "
                "(e.g., [Build] \nSearchFolders = \"path/to/search/folder\"). Exiting to avoid "
                "file "
                "corruption.");
            return -1;
        }

        if (!filesystem::exists(parsed_options->OutputFolder))
        {
            std::error_code error_code;
            filesystem::create_directories(parsed_options->OutputFolder);

            if (error_code)
            {
                HelperFunctions::Log<LogTypes::Error>(
                    "Failed to create output directories with error: {}.\n", error_code.message());
                return -1;
            }
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
            HelperFunctions::Log<LogTypes::Warn>("You have DoFileContentIntegrityChecks set to "
                                                 "true. That feature is currently not "
                                                 "implemented. You can disable this warning by "
                                                 "setting it to false, or commenting "
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
            temporary_dylib_folder, EmbeddedDynamicLibraries);

        if (!dylib_caching_result)
        {
            HelperFunctions::Log<LogTypes::Error>("Failed to cache dynamic library.\n");
            return -1;
        }

        auto slang_compiler_dylib =
            DynamicLibraryLoader::LoadDynamicLibrary(temporary_dylib_folder / "slang-compiler");

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

        if (compile_shaders<Verbose>(
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
