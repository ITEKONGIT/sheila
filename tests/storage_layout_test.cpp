#include "ssheila/core/storage_layout.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>

int main() {
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
                      ("ssheila-storage-test-" + std::to_string(unique));
    const auto layout = ssheila::core::make_storage_layout(root);

    ssheila::core::create_storage_layout(layout);

    assert(std::filesystem::is_directory(layout.objects));
    assert(std::filesystem::is_directory(layout.notes));
    assert(std::filesystem::is_directory(layout.previews));
    assert(std::filesystem::is_directory(layout.uploads));
    assert(std::filesystem::is_directory(layout.versions));
    assert(std::filesystem::is_directory(layout.exports));
    assert(std::filesystem::is_directory(layout.backups));

    ssheila::core::set_active_storage_layout(layout);
    assert(ssheila::core::active_storage_layout().root == root);

    std::filesystem::remove_all(root);
    std::cout << "storage layout test passed\n";
}
