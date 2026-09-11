#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace ssheila::core {

struct ItemRecord {
    std::string id;
    std::string type;
    std::string title;
    std::string content;
    std::optional<std::filesystem::path> objectPath;
    std::string mediaType;
    std::uint64_t byteSize{0};
    std::string checksum;
    std::string createdAt;
    std::string updatedAt;
};

class Database {
public:
    explicit Database(const std::filesystem::path& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

    void migrate();
    [[nodiscard]] ItemRecord create_note(std::string title, std::string content);
    [[nodiscard]] std::optional<ItemRecord> update_note(
        std::string_view id, std::string title, std::string content);
    [[nodiscard]] ItemRecord create_file(
        std::string title,
        const std::filesystem::path& objectPath,
        std::string mediaType,
        std::uint64_t byteSize,
        std::string checksum,
        std::string type = "file");
    [[nodiscard]] std::vector<ItemRecord> list_items(std::string_view query = {});
    [[nodiscard]] std::optional<ItemRecord> get_item(std::string_view id);
    [[nodiscard]] bool delete_item(std::string_view id);

    static void set_active(Database& database);
    [[nodiscard]] static Database& active();

private:
    void execute(const char* sql);
    sqlite3* handle_{nullptr};
    std::mutex mutex_;
    static Database* active_;
};

}  // namespace ssheila::core
