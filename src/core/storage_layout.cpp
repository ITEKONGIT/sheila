#include "ssheila/core/storage_layout.hpp"

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace ssheila::core {
namespace {

std::optional<StorageLayout> activeLayout;

std::filesystem::path environment_path(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::filesystem::path{} : std::filesystem::path{value};
}

}  // namespace

std::filesystem::path default_data_root() {
#ifdef _WIN32
    auto base = environment_path("LOCALAPPDATA");
    if (base.empty()) {
        base = environment_path("USERPROFILE");
    }
    if (base.empty()) {
        throw std::runtime_error("Neither LOCALAPPDATA nor USERPROFILE is available");
    }
    return base / "sSheila";
#else
    auto base = environment_path("XDG_DATA_HOME");
    if (!base.empty()) {
        return base / "ssheila";
    }

    base = environment_path("HOME");
    if (base.empty()) {
        throw std::runtime_error("Neither XDG_DATA_HOME nor HOME is available");
    }
    return base / ".local" / "share" / "ssheila";
#endif
}

StorageLayout make_storage_layout(const std::filesystem::path& root) {
    if (root.empty()) {
        throw std::invalid_argument("The sSheila data root cannot be empty");
    }

    return StorageLayout{
        .root = root,
        .objects = root / "objects",
        .notes = root / "notes",
        .previews = root / "previews",
        .uploads = root / "uploads",
        .versions = root / "versions",
        .exports = root / "exports",
        .backups = root / "backups",
        .database = root / "ssheila.db",
    };
}

void create_storage_layout(const StorageLayout& layout) {
    const std::filesystem::path directories[] = {
        layout.root,
        layout.objects,
        layout.notes,
        layout.previews,
        layout.uploads,
        layout.versions,
        layout.exports,
        layout.backups,
    };

    for (const auto& directory : directories) {
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) {
            throw std::runtime_error(
                "Could not create storage directory '" + directory.string() + "': " + error.message());
        }
    }
}

void set_active_storage_layout(StorageLayout layout) {
    if (activeLayout.has_value()) {
        throw std::logic_error("The active sSheila storage layout is already initialized");
    }
    activeLayout = std::move(layout);
}

const StorageLayout& active_storage_layout() {
    if (!activeLayout.has_value()) {
        throw std::logic_error("The active sSheila storage layout has not been initialized");
    }
    return *activeLayout;
}

}  // namespace ssheila::core
