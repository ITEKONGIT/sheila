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

std::string column_text(sqlite3_stmt* statement, int index) {
    const auto* value = sqlite3_column_text(statement, index);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
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
    return item;
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
    "COALESCE(items.byte_size, 0), COALESCE(items.checksum, ''), items.created_at, items.updated_at";

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

        INSERT OR IGNORE INTO schema_migrations(version) VALUES (1);
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
