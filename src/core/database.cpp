#include "ssheila/core/database.hpp"

#include <sqlite3.h>

#include <array>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace ssheila::core {
namespace {

class Statement {
public:
    Statement(sqlite3* database, const char* sql) {
        if (sqlite3_prepare_v2(database, sql, -1, &handle_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(database));
        }
    }

    ~Statement() {
        sqlite3_finalize(handle_);
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    sqlite3_stmt* get() const {
        return handle_;
    }

private:
    sqlite3_stmt* handle_{nullptr};
};

void bind_text(sqlite3_stmt* statement, int index, std::string_view value) {
    if (sqlite3_bind_text(
            statement, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
        throw std::runtime_error("Could not bind SQLite text value");
    }
}

void bind_optional_text(sqlite3_stmt* statement, int index, const std::optional<std::string>& value) {
    if (value.has_value()) {
        bind_text(statement, index, *value);
        return;
    }
    if (sqlite3_bind_null(statement, index) != SQLITE_OK) {
        throw std::runtime_error("Could not bind SQLite NULL value");
    }
}

std::string column_text(sqlite3_stmt* statement, int index) {
    const auto* value = sqlite3_column_text(statement, index);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

std::optional<std::string> optional_column_text(sqlite3_stmt* statement, int index) {
    if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
        return std::nullopt;
    }
    return column_text(statement, index);
}

ItemRecord read_item(sqlite3_stmt* statement) {
    ItemRecord item;
    item.id = column_text(statement, 0);
    item.type = column_text(statement, 1);
    item.title = column_text(statement, 2);
    item.content = column_text(statement, 3);
    if (sqlite3_column_type(statement, 4) != SQLITE_NULL) {
        item.objectPath = std::filesystem::path{column_text(statement, 4)};
    }
    item.mediaType = column_text(statement, 5);
    item.byteSize = static_cast<std::uint64_t>(sqlite3_column_int64(statement, 6));
    item.checksum = column_text(statement, 7);
    item.createdAt = column_text(statement, 8);
    item.updatedAt = column_text(statement, 9);
    item.deletedAt = optional_column_text(statement, 10);
    return item;
}

TaskRecord read_task(sqlite3_stmt* statement) {
    TaskRecord task;
    task.id = column_text(statement, 0);
    task.title = column_text(statement, 1);
    task.details = column_text(statement, 2);
    task.status = column_text(statement, 3);
    task.priority = sqlite3_column_int(statement, 4);
    task.dueAt = optional_column_text(statement, 5);
    task.reminderAt = optional_column_text(statement, 6);
    task.remindedAt = optional_column_text(statement, 7);
    task.linkedItemId = optional_column_text(statement, 8);
    task.createdAt = column_text(statement, 9);
    task.updatedAt = column_text(statement, 10);
    task.deletedAt = optional_column_text(statement, 11);
    return task;
}

std::string new_id() {
    thread_local std::mt19937_64 generator{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> distribution;
    const std::array<std::uint64_t, 2> values{distribution(generator), distribution(generator)};

    std::ostringstream output;
    output << std::hex << std::setfill('0')
           << std::setw(16) << values[0]
           << std::setw(16) << values[1];
    const auto raw = output.str();
    return raw.substr(0, 8) + "-" + raw.substr(8, 4) + "-" + raw.substr(12, 4) + "-" +
           raw.substr(16, 4) + "-" + raw.substr(20, 12);
}

std::string safe_fts_query(std::string_view query) {
    std::string escaped;
    escaped.reserve(query.size() + 2);
    escaped.push_back('"');
    for (const char character : query) {
        if (character == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(character);
    }
    escaped.push_back('"');
    return escaped;
}

constexpr const char* itemColumns =
    "items.id, items.type, items.title, items.content, items.object_path, items.media_type, "
    "COALESCE(items.byte_size, 0), COALESCE(items.checksum, ''), items.created_at, items.updated_at, items.deleted_at";

constexpr const char* taskColumns =
    "tasks.id, tasks.title, tasks.details, tasks.status, tasks.priority, tasks.due_at, "
    "tasks.reminder_at, tasks.reminded_at, tasks.linked_item_id, tasks.created_at, "
    "tasks.updated_at, tasks.deleted_at";

}  // namespace

Database* Database::active_ = nullptr;

Database::Database(const std::filesystem::path& path) {
    const auto utf8Path = path.string();
    if (sqlite3_open(utf8Path.c_str(), &handle_) != SQLITE_OK) {
        const std::string message = handle_ == nullptr ? "unknown SQLite error" : sqlite3_errmsg(handle_);
        if (handle_ != nullptr) {
            sqlite3_close(handle_);
            handle_ = nullptr;
        }
        throw std::runtime_error("Could not open sSheila metadata database: " + message);
    }

    execute("PRAGMA journal_mode=WAL;");
    execute("PRAGMA foreign_keys=ON;");
    execute("PRAGMA busy_timeout=5000;");
}

Database::~Database() {
    if (handle_ != nullptr) {
        sqlite3_close(handle_);
    }
}

void Database::execute(const char* sql) {
    char* error = nullptr;
    if (sqlite3_exec(handle_, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error == nullptr ? "unknown SQLite error" : error;
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

void Database::migrate() {
    execute(R"sql(
        BEGIN;

        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS items (
            id TEXT PRIMARY KEY,
            type TEXT NOT NULL CHECK (type IN ('note', 'document', 'file', 'image', 'video', 'audio', 'link', 'clipboard')),
            title TEXT NOT NULL DEFAULT '',
            content TEXT NOT NULL DEFAULT '',
            object_path TEXT,
            media_type TEXT,
            byte_size INTEGER,
            checksum TEXT,
            source_device_id TEXT,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            deleted_at TEXT
        );

        CREATE TABLE IF NOT EXISTS tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE COLLATE NOCASE
        );

        CREATE TABLE IF NOT EXISTS item_tags (
            item_id TEXT NOT NULL REFERENCES items(id) ON DELETE CASCADE,
            tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
            PRIMARY KEY (item_id, tag_id)
        );

        CREATE TABLE IF NOT EXISTS transfers (
            id TEXT PRIMARY KEY,
            item_id TEXT REFERENCES items(id) ON DELETE SET NULL,
            state TEXT NOT NULL,
            bytes_received INTEGER NOT NULL DEFAULT 0,
            total_bytes INTEGER,
            temporary_path TEXT,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE VIRTUAL TABLE IF NOT EXISTS item_search USING fts5(
            item_id UNINDEXED,
            title,
            content,
            tokenize='unicode61'
        );

        CREATE TABLE IF NOT EXISTS tasks (
            id TEXT PRIMARY KEY,
            title TEXT NOT NULL,
            details TEXT NOT NULL DEFAULT '',
            status TEXT NOT NULL DEFAULT 'open' CHECK (status IN ('open', 'in_progress', 'completed')),
            priority INTEGER NOT NULL DEFAULT 0 CHECK (priority BETWEEN 0 AND 3),
            due_at TEXT,
            reminder_at TEXT,
            reminded_at TEXT,
            linked_item_id TEXT REFERENCES items(id) ON DELETE SET NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            deleted_at TEXT
        );
        CREATE INDEX IF NOT EXISTS tasks_due_idx
            ON tasks(reminder_at) WHERE deleted_at IS NULL AND reminded_at IS NULL;

        INSERT OR IGNORE INTO schema_migrations(version) VALUES (1);
        INSERT OR IGNORE INTO schema_migrations(version) VALUES (2);
        COMMIT;
    )sql");
}

ItemRecord Database::create_note(std::string title, std::string content) {
    const auto id = new_id();
    {
        std::lock_guard lock{mutex_};
        Statement insert{handle_, "INSERT INTO items(id, type, title, content) VALUES (?, 'note', ?, ?);"};
        bind_text(insert.get(), 1, id);
        bind_text(insert.get(), 2, title);
        bind_text(insert.get(), 3, content);
        if (sqlite3_step(insert.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(handle_));
        }

        Statement index{handle_, "INSERT INTO item_search(item_id, title, content) VALUES (?, ?, ?);"};
        bind_text(index.get(), 1, id);
        bind_text(index.get(), 2, title);
        bind_text(index.get(), 3, content);
        if (sqlite3_step(index.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(handle_));
        }
    }
    return get_item(id).value();
}

std::optional<ItemRecord> Database::update_note(
    std::string_view id, std::string title, std::string content) {
    {
        std::lock_guard lock{mutex_};
        Statement update{
            handle_,
            "UPDATE items SET title = ?, content = ?, updated_at = CURRENT_TIMESTAMP "
            "WHERE id = ? AND type = 'note' AND deleted_at IS NULL;"};
        bind_text(update.get(), 1, title);
        bind_text(update.get(), 2, content);
        bind_text(update.get(), 3, id);
        if (sqlite3_step(update.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(handle_));
        }
        if (sqlite3_changes(handle_) == 0) {
            return std::nullopt;
        }

        Statement removeIndex{handle_, "DELETE FROM item_search WHERE item_id = ?;"};
        bind_text(removeIndex.get(), 1, id);
        if (sqlite3_step(removeIndex.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(handle_));
        }

        Statement addIndex{handle_, "INSERT INTO item_search(item_id, title, content) VALUES (?, ?, ?);"};
        bind_text(addIndex.get(), 1, id);
        bind_text(addIndex.get(), 2, title);
        bind_text(addIndex.get(), 3, content);
        if (sqlite3_step(addIndex.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(handle_));
        }
    }
    return get_item(id);
}

ItemRecord Database::create_file(
    std::string title,
    const std::filesystem::path& objectPath,
    std::string mediaType,
    std::uint64_t byteSize,
    std::string checksum,
    std::string type) {
    const auto id = new_id();
    {
        std::lock_guard lock{mutex_};
        Statement insert{
            handle_,
            "INSERT INTO items(id, type, title, object_path, media_type, byte_size, checksum) "
            "VALUES (?, ?, ?, ?, ?, ?, ?);"};
        bind_text(insert.get(), 1, id);
        bind_text(insert.get(), 2, type);
        bind_text(insert.get(), 3, title);
        bind_text(insert.get(), 4, objectPath.string());
        bind_text(insert.get(), 5, mediaType);
        if (sqlite3_bind_int64(insert.get(), 6, static_cast<sqlite3_int64>(byteSize)) != SQLITE_OK) {
            throw std::runtime_error("Could not bind SQLite file size");
        }
        bind_text(insert.get(), 7, checksum);
        if (sqlite3_step(insert.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(handle_));
        }

        Statement index{handle_, "INSERT INTO item_search(item_id, title, content) VALUES (?, ?, '');"};
        bind_text(index.get(), 1, id);
        bind_text(index.get(), 2, title);
        if (sqlite3_step(index.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(handle_));
        }
    }
    return get_item(id).value();
}

std::vector<ItemRecord> Database::list_items(std::string_view query) {
    std::lock_guard lock{mutex_};
    const bool searching = !query.empty();
    const std::string sql = searching
        ? std::string{"SELECT "} + itemColumns +
              " FROM items JOIN item_search ON item_search.item_id = items.id "
              "WHERE item_search MATCH ? AND deleted_at IS NULL ORDER BY updated_at DESC LIMIT 200;"
        : std::string{"SELECT "} + itemColumns +
              " FROM items WHERE deleted_at IS NULL ORDER BY updated_at DESC LIMIT 200;";
    Statement statement{handle_, sql.c_str()};
    if (searching) {
        bind_text(statement.get(), 1, safe_fts_query(query));
    }

    std::vector<ItemRecord> items;
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(statement.get())) == SQLITE_ROW) {
        items.push_back(read_item(statement.get()));
    }
    if (result != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(handle_));
    }
    return items;
}

std::optional<ItemRecord> Database::get_item(std::string_view id) {
    std::lock_guard lock{mutex_};
    const std::string sql = std::string{"SELECT "} + itemColumns +
                            " FROM items WHERE id = ? AND deleted_at IS NULL;";
    Statement statement{handle_, sql.c_str()};
    bind_text(statement.get(), 1, id);
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    return read_item(statement.get());
}

std::vector<ItemRecord> Database::list_deleted_items() {
    std::lock_guard lock{mutex_};
    const std::string sql = std::string{"SELECT "} + itemColumns +
                            " FROM items WHERE deleted_at IS NOT NULL "
                            "ORDER BY deleted_at DESC LIMIT 200;";
    Statement statement{handle_, sql.c_str()};
    std::vector<ItemRecord> items;
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(statement.get())) == SQLITE_ROW) {
        items.push_back(read_item(statement.get()));
    }
    if (result != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(handle_));
    }
    return items;
}

bool Database::trash_item(std::string_view id) {
    std::lock_guard lock{mutex_};
    Statement statement{handle_,
                        "UPDATE items SET deleted_at = CURRENT_TIMESTAMP "
                        "WHERE id = ? AND deleted_at IS NULL;"};
    bind_text(statement.get(), 1, id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(handle_));
    }
    return sqlite3_changes(handle_) != 0;
}

std::optional<ItemRecord> Database::purge_item(std::string_view id) {
    std::lock_guard lock{mutex_};
    std::optional<ItemRecord> item;
    {
        const std::string sql = std::string{"SELECT "} + itemColumns +
                                " FROM items WHERE id = ? AND deleted_at IS NOT NULL;";
        Statement select{handle_, sql.c_str()};
        bind_text(select.get(), 1, id);
        if (sqlite3_step(select.get()) == SQLITE_ROW) {
            item = read_item(select.get());
        }
    }
    if (!item) {
        return std::nullopt;
    }

    execute("BEGIN;");
    try {
        Statement index{handle_, "DELETE FROM item_search WHERE item_id = ?;"};
        bind_text(index.get(), 1, id);
        if (sqlite3_step(index.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(handle_));
        }
        Statement remove{handle_, "DELETE FROM items WHERE id = ? AND deleted_at IS NOT NULL;"};
        bind_text(remove.get(), 1, id);
        if (sqlite3_step(remove.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(handle_));
        }
        execute("COMMIT;");
    } catch (...) {
        execute("ROLLBACK;");
        throw;
    }
    return item;
}

TaskRecord Database::create_task(
    std::string title,
    std::string details,
    int priority,
    std::optional<std::string> dueAt,
    std::optional<std::string> reminderAt,
    std::optional<std::string> linkedItemId) {
    const auto id = new_id();
    {
        std::lock_guard lock{mutex_};
        Statement insert{handle_,
                         "INSERT INTO tasks(id, title, details, priority, due_at, reminder_at, linked_item_id) "
                         "VALUES (?, ?, ?, ?, ?, ?, ?);"};
        bind_text(insert.get(), 1, id);
        bind_text(insert.get(), 2, title);
        bind_text(insert.get(), 3, details);
        if (sqlite3_bind_int(insert.get(), 4, priority) != SQLITE_OK) {
            throw std::runtime_error("Could not bind task priority");
        }
        bind_optional_text(insert.get(), 5, dueAt);
        bind_optional_text(insert.get(), 6, reminderAt);
        bind_optional_text(insert.get(), 7, linkedItemId);
        if (sqlite3_step(insert.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(handle_));
        }
    }
    std::lock_guard lock{mutex_};
    const std::string sql = std::string{"SELECT "} + taskColumns + " FROM tasks WHERE id = ?;";
    Statement select{handle_, sql.c_str()};
    bind_text(select.get(), 1, id);
    if (sqlite3_step(select.get()) != SQLITE_ROW) {
        throw std::runtime_error("Could not read created task");
    }
    return read_task(select.get());
}

std::optional<TaskRecord> Database::update_task(
    std::string_view id,
    std::string title,
    std::string details,
    int priority,
    std::optional<std::string> dueAt,
    std::optional<std::string> reminderAt,
    std::optional<std::string> linkedItemId) {
    {
        std::lock_guard lock{mutex_};
        Statement update{handle_,
                         "UPDATE tasks SET title = ?, details = ?, priority = ?, due_at = ?, "
                         "reminder_at = ?, linked_item_id = ?, reminded_at = NULL, "
                         "updated_at = CURRENT_TIMESTAMP WHERE id = ? AND deleted_at IS NULL;"};
        bind_text(update.get(), 1, title);
        bind_text(update.get(), 2, details);
        if (sqlite3_bind_int(update.get(), 3, priority) != SQLITE_OK) {
            throw std::runtime_error("Could not bind task priority");
        }
        bind_optional_text(update.get(), 4, dueAt);
        bind_optional_text(update.get(), 5, reminderAt);
        bind_optional_text(update.get(), 6, linkedItemId);
        bind_text(update.get(), 7, id);
        if (sqlite3_step(update.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(handle_));
        }
        if (sqlite3_changes(handle_) == 0) {
            return std::nullopt;
        }
    }
    std::lock_guard lock{mutex_};
    const std::string sql = std::string{"SELECT "} + taskColumns + " FROM tasks WHERE id = ?;";
    Statement select{handle_, sql.c_str()};
    bind_text(select.get(), 1, id);
    if (sqlite3_step(select.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    return read_task(select.get());
}

std::vector<TaskRecord> Database::list_tasks(bool includeDeleted) {
    std::lock_guard lock{mutex_};
    const std::string sql = std::string{"SELECT "} + taskColumns +
        (includeDeleted ? " FROM tasks ORDER BY CASE WHEN status = 'completed' THEN 1 ELSE 0 END, "
                           "COALESCE(due_at, '9999-12-31T23:59') LIMIT 200;"
                        : " FROM tasks WHERE deleted_at IS NULL ORDER BY CASE WHEN status = 'completed' THEN 1 ELSE 0 END, "
                          "COALESCE(due_at, '9999-12-31T23:59') LIMIT 200;");
    Statement statement{handle_, sql.c_str()};
    std::vector<TaskRecord> tasks;
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(statement.get())) == SQLITE_ROW) {
        tasks.push_back(read_task(statement.get()));
    }
    if (result != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(handle_));
    }
    return tasks;
}

std::vector<TaskRecord> Database::claim_due_tasks() {
    constexpr int maxBatch = 32;
    std::lock_guard lock{mutex_};
    const std::string sql = std::string{"SELECT "} + taskColumns +
        " FROM tasks WHERE deleted_at IS NULL AND status != 'completed' AND reminded_at IS NULL "
        "AND reminder_at IS NOT NULL AND datetime(reminder_at) <= CURRENT_TIMESTAMP "
        "ORDER BY reminder_at LIMIT " + std::to_string(maxBatch) + ";";
    Statement select{handle_, sql.c_str()};
    std::vector<TaskRecord> due;
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(select.get())) == SQLITE_ROW) {
        due.push_back(read_task(select.get()));
    }
    if (result != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(handle_));
    }

    for (const auto& task : due) {
        Statement claim{handle_,
                        "UPDATE tasks SET reminded_at = CURRENT_TIMESTAMP, updated_at = CURRENT_TIMESTAMP "
                        "WHERE id = ? AND reminded_at IS NULL AND deleted_at IS NULL;"};
        bind_text(claim.get(), 1, task.id);
        if (sqlite3_step(claim.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(handle_));
        }
    }
    return due;
}

bool Database::complete_task(std::string_view id) {
    std::lock_guard lock{mutex_};
    Statement statement{handle_,
                        "UPDATE tasks SET status = 'completed', reminded_at = NULL, "
                        "updated_at = CURRENT_TIMESTAMP WHERE id = ? AND deleted_at IS NULL;"};
    bind_text(statement.get(), 1, id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(handle_));
    }
    return sqlite3_changes(handle_) != 0;
}

bool Database::trash_task(std::string_view id) {
    std::lock_guard lock{mutex_};
    Statement statement{handle_, "UPDATE tasks SET deleted_at = CURRENT_TIMESTAMP WHERE id = ? AND deleted_at IS NULL;"};
    bind_text(statement.get(), 1, id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(handle_));
    }
    return sqlite3_changes(handle_) != 0;
}

bool Database::restore_task(std::string_view id) {
    std::lock_guard lock{mutex_};
    Statement statement{handle_, "UPDATE tasks SET deleted_at = NULL, updated_at = CURRENT_TIMESTAMP WHERE id = ? AND deleted_at IS NOT NULL;"};
    bind_text(statement.get(), 1, id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(handle_));
    }
    return sqlite3_changes(handle_) != 0;
}

bool Database::purge_task(std::string_view id) {
    std::lock_guard lock{mutex_};
    Statement statement{handle_, "DELETE FROM tasks WHERE id = ? AND deleted_at IS NOT NULL;"};
    bind_text(statement.get(), 1, id);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(handle_));
    }
    return sqlite3_changes(handle_) != 0;
}

bool Database::restore_item(std::string_view id) {
    std::lock_guard lock{mutex_};
    Statement update{
        handle_,
        "UPDATE items SET deleted_at = NULL, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ? AND deleted_at IS NOT NULL;"};
    bind_text(update.get(), 1, id);
    if (sqlite3_step(update.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(handle_));
    }
    return sqlite3_changes(handle_) > 0;
}

void Database::set_active(Database& database) {
    if (active_ != nullptr) {
        throw std::logic_error("The active sSheila database is already initialized");
    }
    active_ = &database;
}

Database& Database::active() {
    if (active_ == nullptr) {
        throw std::logic_error("The active sSheila database has not been initialized");
    }
    return *active_;
}

}  // namespace ssheila::core
