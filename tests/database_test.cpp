#include "ssheila/core/database.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

int main() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto databasePath = std::filesystem::temp_directory_path() /
                              ("ssheila-database-test-" + std::to_string(stamp) + ".db");
    std::error_code cleanupError;
    std::filesystem::remove(databasePath, cleanupError);
    std::filesystem::remove(databasePath.string() + "-wal", cleanupError);
    std::filesystem::remove(databasePath.string() + "-shm", cleanupError);

    {
        ssheila::core::Database database{databasePath};
        database.migrate();

        const auto note = database.create_note("Test note", "Persistent content");
        assert(database.get_item(note.id).has_value());
        assert(database.trash_item(note.id));
        assert(!database.get_item(note.id).has_value());
        assert(database.list_deleted_items().size() == 1);
        assert(database.restore_item(note.id));
        assert(database.get_item(note.id).has_value());

        const auto task = database.create_task(
            "Test task", "Reminder content", 2, std::nullopt,
            std::optional<std::string>{"2000-01-01T00:00:00Z"}, note.id);
        assert(database.list_tasks().size() == 1);
        const auto due = database.claim_due_tasks();
        assert(due.size() == 1);
        assert(due.front().id == task.id);
        assert(database.complete_task(task.id));
        assert(database.trash_task(task.id));
        assert(database.list_tasks().empty());
        assert(database.purge_task(task.id));
        assert(!database.purge_task(task.id));

        assert(database.trash_item(note.id));
        const auto deleted = database.purge_item(note.id);
        assert(deleted.has_value());
        assert(!database.purge_item(note.id).has_value());
    }

    std::filesystem::remove(databasePath, cleanupError);
    std::filesystem::remove(databasePath.string() + "-wal", cleanupError);
    std::filesystem::remove(databasePath.string() + "-shm", cleanupError);
    return 0;
}
