#include "ssheila/http/tasks_controller.hpp"

#include "ssheila/http/events_websocket.hpp"

#include "ssheila/core/database.hpp"

#include <json/json.h>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ssheila::http {
namespace {

drogon::HttpResponsePtr error_response(
    drogon::HttpStatusCode status, std::string_view code, std::string_view message) {
    Json::Value body;
    body["error"] = std::string{code};
    body["message"] = std::string{message};
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    return response;
}

Json::Value task_json(const ssheila::core::TaskRecord& task) {
    Json::Value value;
    value["id"] = task.id;
    value["title"] = task.title;
    value["details"] = task.details;
    value["status"] = task.status;
    value["priority"] = task.priority;
    value["createdAt"] = task.createdAt;
    value["updatedAt"] = task.updatedAt;
    value["dueAt"] = task.dueAt ? Json::Value{*task.dueAt} : Json::Value{Json::nullValue};
    value["reminderAt"] = task.reminderAt ? Json::Value{*task.reminderAt} : Json::Value{Json::nullValue};
    value["remindedAt"] = task.remindedAt ? Json::Value{*task.remindedAt} : Json::Value{Json::nullValue};
    value["linkedItemId"] = task.linkedItemId ? Json::Value{*task.linkedItemId} : Json::Value{Json::nullValue};
    value["deletedAt"] = task.deletedAt ? Json::Value{*task.deletedAt} : Json::Value{Json::nullValue};
    return value;
}

std::optional<std::string> optional_string(const Json::Value& body, const char* name) {
    if (!body.isMember(name) || body[name].isNull()) {
        return std::nullopt;
    }
    if (!body[name].isString()) {
        throw std::invalid_argument(std::string{name} + " must be a string or null");
    }
    const auto value = body[name].asString();
    return value.empty() ? std::nullopt : std::optional<std::string>{value};
}

struct TaskInput {
    std::string title;
    std::string details;
    int priority{0};
    std::optional<std::string> dueAt;
    std::optional<std::string> reminderAt;
    std::optional<std::string> linkedItemId;
};

TaskInput read_input(const drogon::HttpRequestPtr& request) {
    const auto body = request->getJsonObject();
    if (!body || !(*body)["title"].isString()) {
        throw std::invalid_argument("A task title string is required");
    }
    TaskInput input;
    input.title = (*body)["title"].asString();
    input.details = (*body)["details"].isString() ? (*body)["details"].asString() : "";
    if (input.title.empty() || input.title.size() > 180) {
        throw std::invalid_argument("Task title must contain 1 to 180 characters");
    }
    if (input.details.size() > 10000) {
        throw std::invalid_argument("Task details are limited to 10000 characters");
    }
    if ((*body).isMember("priority")) {
        if (!(*body)["priority"].isInt()) {
            throw std::invalid_argument("Task priority must be an integer from 0 to 3");
        }
        input.priority = std::clamp((*body)["priority"].asInt(), 0, 3);
    }
    input.dueAt = optional_string(*body, "dueAt");
    input.reminderAt = optional_string(*body, "reminderAt");
    input.linkedItemId = optional_string(*body, "linkedItemId");
    return input;
}

}  // namespace

void TasksController::list(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        const auto tasks = ssheila::core::Database::active().list_tasks();
        Json::Value body;
        body["count"] = Json::UInt64(tasks.size());
        body["tasks"] = Json::arrayValue;
        for (const auto& task : tasks) {
            body["tasks"].append(task_json(task));
        }
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "tasks_failed", error.what()));
    }
}

void TasksController::create(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        const auto input = read_input(request);
        const auto task = ssheila::core::Database::active().create_task(
            input.title, input.details, input.priority, input.dueAt, input.reminderAt, input.linkedItemId);
        auto response = drogon::HttpResponse::newHttpJsonResponse(task_json(task));
        response->setStatusCode(drogon::k201Created);
        callback(response);
        EventsWebSocket::publish(R"({"type":"tasks.changed"})");
    } catch (const std::invalid_argument& error) {
        callback(error_response(drogon::k400BadRequest, "invalid_task", error.what()));
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "task_create_failed", error.what()));
    }
}

void TasksController::update(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string id) const {
    try {
        const auto input = read_input(request);
        const auto task = ssheila::core::Database::active().update_task(
            id, input.title, input.details, input.priority, input.dueAt, input.reminderAt, input.linkedItemId);
        if (!task) {
            callback(error_response(drogon::k404NotFound, "task_not_found", "The task does not exist"));
            return;
        }
        callback(drogon::HttpResponse::newHttpJsonResponse(task_json(*task)));
        EventsWebSocket::publish(R"({"type":"tasks.changed"})");
    } catch (const std::invalid_argument& error) {
        callback(error_response(drogon::k400BadRequest, "invalid_task", error.what()));
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "task_update_failed", error.what()));
    }
}

void TasksController::complete(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string id) const {
    try {
        if (!ssheila::core::Database::active().complete_task(id)) {
            callback(error_response(drogon::k404NotFound, "task_not_found", "The task does not exist"));
            return;
        }
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k204NoContent);
        callback(response);
        EventsWebSocket::publish(R"({"type":"tasks.changed"})");
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "task_complete_failed", error.what()));
    }
}

void TasksController::trash(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string id) const {
    try {
        if (!ssheila::core::Database::active().trash_task(id)) {
            callback(error_response(drogon::k404NotFound, "task_not_found", "The task does not exist"));
            return;
        }
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k204NoContent);
        callback(response);
        EventsWebSocket::publish(R"({"type":"tasks.changed"})");
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "task_trash_failed", error.what()));
    }
}

}  // namespace ssheila::http
