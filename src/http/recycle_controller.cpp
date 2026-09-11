#include "ssheila/http/recycle_controller.hpp"

#include "ssheila/core/database.hpp"
#include "ssheila/http/events_websocket.hpp"

#include <json/json.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace ssheila::http {
namespace {

Json::Value item_json(const ssheila::core::ItemRecord& item) {
    Json::Value value;
    value["id"] = item.id;
    value["type"] = item.type;
    value["title"] = item.title;
    value["mediaType"] = item.mediaType;
    value["byteSize"] = Json::UInt64(item.byteSize);
    value["deletedAt"] = item.deletedAt ? Json::Value{*item.deletedAt} : Json::Value{Json::nullValue};
    return value;
}

Json::Value task_json(const ssheila::core::TaskRecord& task) {
    Json::Value value;
    value["id"] = task.id;
    value["title"] = task.title;
    value["details"] = task.details;
    value["status"] = task.status;
    value["priority"] = task.priority;
    value["deletedAt"] = task.deletedAt ? Json::Value{*task.deletedAt} : Json::Value{Json::nullValue};
    return value;
}

drogon::HttpResponsePtr error_response(
    drogon::HttpStatusCode status, std::string_view code, std::string_view message) {
    Json::Value body;
    body["error"] = std::string{code};
    body["message"] = std::string{message};
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    return response;
}

drogon::HttpResponsePtr no_content() {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k204NoContent);
    return response;
}

}  // namespace

void RecycleController::list(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        const auto items = ssheila::core::Database::active().list_deleted_items();
        const auto tasks = ssheila::core::Database::active().list_tasks(true);
        Json::Value body;
        body["items"] = Json::arrayValue;
        body["tasks"] = Json::arrayValue;
        for (const auto& item : items) {
            body["items"].append(item_json(item));
        }
        for (const auto& task : tasks) {
            if (task.deletedAt) {
                body["tasks"].append(task_json(task));
            }
        }
        body["count"] = Json::UInt64(items.size() + body["tasks"].size());
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "recycle_bin_failed", error.what()));
    }
}

void RecycleController::restoreItem(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string id) const {
    try {
        if (!ssheila::core::Database::active().restore_item(id)) {
            callback(error_response(drogon::k404NotFound, "item_not_found", "The recycle-bin item does not exist"));
            return;
        }
        callback(no_content());
        EventsWebSocket::publish(R"({"type":"items.changed"})");
        EventsWebSocket::publish(R"({"type":"recycle.changed"})");
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "item_restore_failed", error.what()));
    }
}

void RecycleController::purgeItem(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string id) const {
    try {
        const auto item = ssheila::core::Database::active().purge_item(id);
        if (!item) {
            callback(error_response(drogon::k404NotFound, "item_not_found", "The recycle-bin item does not exist"));
            return;
        }
        if (item->objectPath) {
            std::error_code error;
            std::filesystem::remove(*item->objectPath, error);
            if (error) {
                // Metadata is already gone; report the orphan so an operator can
                // clean it up without silently hiding a storage failure.
                callback(error_response(drogon::k500InternalServerError, "object_purge_failed", error.message()));
                return;
            }
        }
        callback(no_content());
        EventsWebSocket::publish(R"({"type":"recycle.changed"})");
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "item_purge_failed", error.what()));
    }
}

void RecycleController::restoreTask(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string id) const {
    try {
        if (!ssheila::core::Database::active().restore_task(id)) {
            callback(error_response(drogon::k404NotFound, "task_not_found", "The recycle-bin task does not exist"));
            return;
        }
        callback(no_content());
        EventsWebSocket::publish(R"({"type":"tasks.changed"})");
        EventsWebSocket::publish(R"({"type":"recycle.changed"})");
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "task_restore_failed", error.what()));
    }
}

void RecycleController::purgeTask(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string id) const {
    try {
        if (!ssheila::core::Database::active().purge_task(id)) {
            callback(error_response(drogon::k404NotFound, "task_not_found", "The recycle-bin task does not exist"));
            return;
        }
        callback(no_content());
        EventsWebSocket::publish(R"({"type":"recycle.changed"})");
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "task_purge_failed", error.what()));
    }
}

}  // namespace ssheila::http
