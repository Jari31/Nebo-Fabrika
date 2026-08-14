#pragma once

#include "HelperFunctions.hpp"
#include "Libraries/include/ankerl/unordered_dense.h"
#include "Libraries/include/xxhash.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string_view>
#include <vector>

namespace JSlang::HashMap
{
namespace filesystem = std::filesystem;
using HashMap        = ankerl::unordered_dense::map<uint64_t, uint64_t>;
using LogTypes       = HelperFunctions::LogTypes;

uint64_t HashString(std::string_view String, uint64_t Seed)
{
    return XXH64(String.data(), String.size(), Seed);
}

void SaveHashMapToDisk(const filesystem::path &FilePath, const HashMap &Map)
{
    std::ofstream hash_map_file(FilePath, std::ios::binary);

    if (!hash_map_file)
    {
        HelperFunctions::Log<LogTypes::Error>("Failed to open hash map file to write to.\n");
    }

    uint64_t hash_map_size = Map.size();
    hash_map_file.write(reinterpret_cast<const char *>(&hash_map_file), sizeof(hash_map_size));

    hash_map_file.write(
        reinterpret_cast<const char *>(Map.values().data()),
        hash_map_size * sizeof(HashMap::value_type)); // NOLINT
}

HashMap LoadHashMapFromDisk(const filesystem::path &FilePath)
{
    std::ifstream hash_map_file(FilePath, std::ios::binary);

    uint64_t hash_map_size = 0;
    hash_map_file.read(reinterpret_cast<char *>(&hash_map_size), sizeof(hash_map_size));

    std::vector<HashMap::value_type> raw_hash_map_values(hash_map_size);
    hash_map_file.read(
        reinterpret_cast<char *>(raw_hash_map_values.data()),
        hash_map_size * sizeof(HashMap::value_type)); // NOLINT

    HashMap map;
    map.reserve(hash_map_size);
    for (auto &&KeyValuePair : raw_hash_map_values)
    {
        map.insert(KeyValuePair);
    }

    return map;
}
} // namespace JSlang::HashMap
