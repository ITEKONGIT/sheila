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
    std::optional<std::string> deletedAt;
};

struct TaskRecord {
    std::string id;
    std::string title;
    std::string details;
    std::string status;
    int priority{0};
    std::optional<std::string> dueAt;
    std::optional<std::string> reminderAt;
    std::optional<std::string> remindedAt;
    std::optional<std::string> linkedItemId;
    std::string createdAt;
    std::string updatedAt;
    std::optional<std::string> deletedAt;
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
    [[nodiscard]] std::vector<ItemRecord> list_deleted_items();
    [[nodiscard]] bool trash_item(std::string_view id);
    [[nodiscard]] bool restore_item(std::string_view id);
    [[nodiscard]] std::optional<ItemRecord> purge_item(std::string_view id);

    [[nodiscard]] TaskRecord create_task(
        std::string title,
        std::string details,
        int priority,
        std::optional<std::string> dueAt,
        std::optional<std::string> reminderAt,
        std::optional<std::string> linkedItemId);
    [[nodiscard]] std::optional<TaskRecord> update_task(
        std::string_view id,
        std::string title,
        std::string details,
        int priority,
        std::optional<std::string> dueAt,
        std::optional<std::string> reminderAt,
        std::optional<std::string> linkedItemId);
    [[nodiscard]] std::vector<TaskRecord> list_tasks(bool includeDeleted = false);
    [[nodiscard]] std::vector<TaskRecord> claim_due_tasks();
    [[nodiscard]] bool complete_task(std::string_view id);
    [[nodiscard]] bool trash_task(std::string_view id);
    [[nodiscard]] bool restore_task(std::string_view id);
    [[nodiscard]] bool purge_task(std::string_view id);

    static void set_active(Database& database);
    [[nodiscard]] static Database& active();

private:
    void execute(const char* sql);
    sqlite3* handle_{nullptr};
    std::mutex mutex_;
    static Database* active_;
};

}  // namespace ssheila::core
