#pragma once

#include <filesystem>

namespace ssheila::core {

struct StorageLayout {
    std::filesystem::path root;
    std::filesystem::path objects;
    std::filesystem::path notes;
    std::filesystem::path previews;
    std::filesystem::path uploads;
    std::filesystem::path versions;
    std::filesystem::path exports;
    std::filesystem::path backups;
    std::filesystem::path database;
};

[[nodiscard]] std::filesystem::path default_data_root();
[[nodiscard]] StorageLayout make_storage_layout(const std::filesystem::path& root);
void create_storage_layout(const StorageLayout& layout);
void set_active_storage_layout(StorageLayout layout);
[[nodiscard]] const StorageLayout& active_storage_layout();

}  // namespace ssheila::core
