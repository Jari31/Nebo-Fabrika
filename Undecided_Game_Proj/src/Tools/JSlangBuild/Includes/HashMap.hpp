#pragma once

#include "HelperFunctions.hpp"
#include "Libraries/include/ankerl/unordered_dense.h"
#include "Libraries/include/xxhash.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>

namespace JSlang::HashMap
{
namespace filesystem = std::filesystem;
using HashMap        = ankerl::unordered_dense::map<uint64_t, uint64_t>;
using LogTypes       = HelperFunctions::LogTypes;

uint64_t HashString(std::string_view String)
{
    return XXH64(String.data(), String.size(), 0);
}

uint64_t HashUINT64(uint64_t Value)
{
    return XXH64(&Value, sizeof(Value), 0);
}

void SaveHashMapToDisk(const filesystem::path &FilePath, const HashMap &Map)
{
    auto temporary_name = std::format("{}.temp", FilePath.string());

    std::ofstream hash_map_file(temporary_name, std::ios::binary);

    if (!hash_map_file)
    {
        std::error_code error_code(errno, std::generic_category());
        HelperFunctions::Log<LogTypes::Error>(
            "Failed to open hash map file to write to at file path: {}. Error: {}\n",
            FilePath.string(),
            error_code.message());
        return;
    }

    uint64_t hash_map_size = Map.size();

    hash_map_file.write(
        reinterpret_cast<const char *>(Map.values().data()),
        hash_map_size * sizeof(HashMap::value_type)); // NOLINT

    if (!hash_map_file)
    {
        std::error_code error_code(errno, std::generic_category());
        HelperFunctions::Log<LogTypes::Error>(
            "Failed to write hash map values to file {}. Error: {}\n",
            FilePath.string(),
            error_code.message());
        return;
    }

    hash_map_file.close();

    if (hash_map_file.good())
    {
        std::error_code error_code;
        std::filesystem::rename(temporary_name, FilePath, error_code);
        if (error_code)
        {
            HelperFunctions::Log<LogTypes::Error>(
                "Failed to rename temporary file. Error: ", error_code.message());
        }
    }
    else
    {
        HelperFunctions::Log<LogTypes::Error>(
            "The hash map containing file cache info is corrupt. Attempting to remove it to avoid "
            "further corruption...");

        std::error_code error_code;
        std::filesystem::remove(temporary_name, error_code);

        if (error_code)
        {
            HelperFunctions::Log("Failed to remove temporary file. Error: ", error_code.message());
        }
    }
}

HashMap LoadHashMapFromDisk(const filesystem::path &FilePath)
{
    std::ifstream hash_map_file(FilePath, std::ios::binary | std::ios::ate);

    if (!hash_map_file)
    {
        std::error_code error_code(errno, std::generic_category());
        HelperFunctions::Log<LogTypes::Error>(
            "Failed to open hashmap file {}. Error: {}\n", FilePath.string(), error_code.message());
        return {};
    }

    uint64_t hash_map_size = hash_map_file.tellg();
    hash_map_file.seekg(0, std::ios::beg);

    constexpr size_t entry_size = sizeof(uint64_t) + sizeof(uint64_t); // 16 bytes

    if (hash_map_size % entry_size != 0)
    {
        HelperFunctions::Log<LogTypes::Error>(
            "Corrupted hashmap file {}: file size ({}) is not a multiple of entry size ({}).\n",
            FilePath.string(),
            hash_map_size,
            entry_size);
        return {};
    }

    uint64_t number_of_entries = hash_map_size / entry_size;

    HashMap map;

    try
    {
        map.reserve(hash_map_size);
    }
    catch (const std::bad_alloc &e)
    {
        HelperFunctions::Log<LogTypes::Error>(
            "Failed to allocate memory for hashmap from file {}. Error: {}\n",
            FilePath.string(),
            e.what());
        return {};
    }

    for (uint64_t i = 0; i < number_of_entries; i++)
    {
        uint64_t key, value; // NOLINT

        hash_map_file.read(reinterpret_cast<char *>(&key), sizeof(key));
        if (!hash_map_file)
        {
            std::string reason = "Unknown stream error";
            if (hash_map_file.eof())
            {
                reason = "Unexpected End-of-File (file truncated or size mismatch)";
            }
            else if (hash_map_file.bad())
            {
                reason = "Fatal I/O error (hardware/system failure)";
            }
            else if (hash_map_file.fail())
            {
                reason = "Logical I/O error (formatting/type conversion mismatch)";
            }

            HelperFunctions::Log<LogTypes::Error>(
                "Failed to read key at index {} from hashmap file {}. Reason: {}\n",
                i,
                FilePath.string(),
                reason);
            return {};
        }

        hash_map_file.read(reinterpret_cast<char *>(&value), sizeof(value));
        if (!hash_map_file)
        {
            std::error_code error_code(errno, std::generic_category());
            HelperFunctions::Log<LogTypes::Error>(
                "Failed to read value at index {} from hashmap file {}. Error: {}\n",
                i,
                FilePath.string(),
                error_code.message());
            return {};
        }

        map.emplace(key, value);
    }

    hash_map_file.close();
    return map;
}
} // namespace JSlang::HashMap
