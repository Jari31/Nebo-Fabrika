#pragma once
#include "HelperFunctions.hpp"
#include "Libraries/include/enkiTS/TaskScheduler.h"
#include <cstddef>
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

std::vector<filesystem::path> MultithreadedDirectoryWalker(
    enki::TaskScheduler            &TaskScheduler,
    const filesystem::path         &SearchDirectory,
    const std::vector<std::string> &WhiteListedExtensions)
{
    auto                          thread_count = TaskScheduler.GetNumTaskThreads();
    std::vector<filesystem::path> file_path_output_buffer;

    std::vector<filesystem::path> current_pass_path_buffer;
    std::vector<filesystem::path> next_pass_path_buffer;

    current_pass_path_buffer.push_back(SearchDirectory);

    auto AppendToFilePathOutputBuffer =
        [&](const filesystem::path &Path, std::vector<filesystem::path> &AppendToBuffer)
    {
        if (WhiteListedExtensions.size() > 0)
        {
            for (const auto &Extension : WhiteListedExtensions)
            {
                if (Path.extension() != Extension)
                {
                    return;
                }
            }
        }

        AppendToBuffer.push_back(Path);
    };

    while (!current_pass_path_buffer.empty())
    {
        using ThreadGlobalPathStorage = std::vector<std::vector<filesystem::path>>;
        ThreadGlobalPathStorage thread_global_directory_output_buffer(thread_count);
        ThreadGlobalPathStorage thread_global_file_path_output_buffer(thread_count);

        enki::TaskSet task(
            static_cast<uint32_t>(current_pass_path_buffer.size()),
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
                        current_pass_path_buffer[i],
                        filesystem::directory_options::skip_permission_denied,
                        error_code);

                    if (error_code)
                    {
                        HelperFunctions::Log<LogTypes::Error>(
                            "Whilst walking directory, ran into error: {}", error_code.message());
                        return {};
                    }

                    for (const auto &entry : iterator)
                    {

                        if (entry.is_directory())
                        {
                            thread_local_directory_output_buffer.push_back(entry.path());
                        }
                        else
                        {

                            AppendToFilePathOutputBuffer(
                                entry.path(), thread_local_file_path_output_buffer);
                        }
                    }
                }
            });

        TaskScheduler.AddTaskSetToPipe(&task);
        TaskScheduler.WaitforTask(&task);

        next_pass_path_buffer.clear();
        for (uint32_t ThreadIndex = 0; ThreadIndex < thread_count; ThreadIndex++)
        {
            next_pass_path_buffer.insert(
                next_pass_path_buffer.end(),
                std::make_move_iterator(thread_global_directory_output_buffer[ThreadIndex].begin()),
                std::make_move_iterator(thread_global_directory_output_buffer[ThreadIndex].end()));
            file_path_output_buffer.insert(
                file_path_output_buffer.end(),
                std::make_move_iterator(thread_global_file_path_output_buffer[ThreadIndex].begin()),
                std::make_move_iterator(thread_global_file_path_output_buffer[ThreadIndex].end()));
        }

        std::swap(current_pass_path_buffer, next_pass_path_buffer);
    }

    return file_path_output_buffer;
}
} // namespace JSlang::DirectoryWalker
