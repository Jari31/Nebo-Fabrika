#pragma once
#include "HelperFunctions.hpp"
#include "Libraries/include/enkiTS/TaskScheduler.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace JSlang::DirectoryWalker
{
using LogTypes       = HelperFunctions::LogTypes;
namespace filesystem = std::filesystem;
template <bool WhitelistCheck = true>
std::vector<filesystem::path> MultithreadedFileGlobber(
    enki::TaskScheduler            &TaskScheduler,
    const filesystem::path         &SearchDirectory,
    const std::vector<std::string> &WhiteListedExtensions)
{
    auto                          thread_count = TaskScheduler.GetNumTaskThreads();
    std::vector<filesystem::path> file_path_output_buffer;

    std::vector<filesystem::path> directories_to_check_in_current_pass;
    std::vector<filesystem::path> directories_to_check_in_next_pass;

    directories_to_check_in_current_pass.push_back(SearchDirectory);

    auto check_if_file_is_whitelisted = [&](const filesystem::path &Path)
    {
        if (WhiteListedExtensions.empty())
        {
            return true;
        }

        return std::any_of(
            WhiteListedExtensions.begin(),
            WhiteListedExtensions.end(),
            [&Path](const auto &Extension) { return Path.extension() == Extension; });
    };

    while (!directories_to_check_in_current_pass.empty())
    {
        using ThreadGlobalPathStorage = std::vector<std::vector<filesystem::path>>;
        ThreadGlobalPathStorage thread_global_directory_output_buffer(thread_count);
        ThreadGlobalPathStorage thread_global_file_path_output_buffer(thread_count);

        enki::TaskSet task(
            static_cast<uint32_t>(directories_to_check_in_current_pass.size()),
            [&](enki::TaskSetPartition Range, uint32_t ThreadIndex) -> void
            {
                auto &thread_local_directory_output_buffer =
                    thread_global_directory_output_buffer[ThreadIndex];
                auto &thread_local_file_path_output_buffer =
                    thread_global_file_path_output_buffer[ThreadIndex];

                for (uint32_t i = Range.start; i < Range.end; i++)
                {
                    std::error_code                error_code;
                    filesystem::directory_iterator iterator(
                        directories_to_check_in_current_pass[i],
                        filesystem::directory_options::skip_permission_denied,
                        error_code);

                    if (error_code)
                    {
                        // HelperFunctions::Log<LogTypes::Error>(
                        //     "Whilst walking directory, ran into error: {}",
                        //     error_code.message());
                        return;
                    }

                    for (const auto &entry : iterator)
                    {

                        if (entry.is_directory())
                        {
                            thread_local_directory_output_buffer.push_back(entry.path());
                        }
                        else
                        {
                            if constexpr (WhitelistCheck)
                            {
                                auto is_whitelisted = check_if_file_is_whitelisted(entry.path());
                                if (!is_whitelisted) continue; // NOLINT
                            }

                            thread_local_file_path_output_buffer.push_back(entry.path());
                        }
                    }
                }
            });

        TaskScheduler.AddTaskSetToPipe(&task);
        TaskScheduler.WaitforTask(&task);

        directories_to_check_in_next_pass.clear();
        for (uint32_t ThreadIndex = 0; ThreadIndex < thread_count; ThreadIndex++)
        {
            std::ranges::move(
                thread_global_directory_output_buffer[ThreadIndex],
                std::back_inserter(directories_to_check_in_next_pass));
            std::ranges::move(
                thread_global_file_path_output_buffer[ThreadIndex],
                std::back_inserter(file_path_output_buffer));
        }

        std::swap(directories_to_check_in_current_pass, directories_to_check_in_next_pass);
    }

    return file_path_output_buffer;
}
} // namespace JSlang::DirectoryWalker
