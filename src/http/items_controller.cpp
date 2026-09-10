#include "ssheila/http/items_controller.hpp"
#include "ssheila/http/events_websocket.hpp"

#include "ssheila/core/database.hpp"
#include "ssheila/core/storage_layout.hpp"

#include <drogon/MultiPart.h>
#include <json/json.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ssheila::http {
namespace {

Json::Value item_json(const ssheila::core::ItemRecord& item) {
    Json::Value value;
    value["id"] = item.id;
    value["type"] = item.type;
    value["title"] = item.title;
    value["content"] = item.content;
    value["mediaType"] = item.mediaType;
    value["byteSize"] = Json::UInt64(item.byteSize);
    value["checksum"] = item.checksum;
    value["createdAt"] = item.createdAt;
    value["updatedAt"] = item.updatedAt;
    if (item.type == "file") {
        value["downloadUrl"] = "/api/v1/files/" + item.id;
    }
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

std::string mime_for_file(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".gif") return "image/gif";
    if (extension == ".webp") return "image/webp";
    if (extension == ".pdf") return "application/pdf";
    if (extension == ".txt" || extension == ".md") return "text/plain";
    if (extension == ".mp3") return "audio/mpeg";
    if (extension == ".mp4") return "video/mp4";
    return "application/octet-stream";
}

std::string safe_file_name(std::string_view original) {
    auto name = std::filesystem::path{original}.filename().string();
    if (name.empty() || name == "." || name == "..") {
        name = "upload.bin";
    }
    return name;
}

}  // namespace

void ItemsController::list(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        const auto items = ssheila::core::Database::active().list_items(request->getParameter("q"));
        Json::Value body;
        body["count"] = Json::UInt64(items.size());
        body["items"] = Json::arrayValue;
        for (const auto& item : items) {
            body["items"].append(item_json(item));
        }
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "items_failed", error.what()));
    }
}

void ItemsController::createNote(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        const auto body = request->getJsonObject();
        if (!body || !(*body)["content"].isString()) {
            callback(error_response(drogon::k400BadRequest, "invalid_note", "A note content string is required"));
            return;
        }
        auto title = (*body)["title"].asString();
        if (title.empty()) {
            title = "Untitled note";
        }
        const auto item = ssheila::core::Database::active().create_note(title, (*body)["content"].asString());
        auto response = drogon::HttpResponse::newHttpJsonResponse(item_json(item));
        response->setStatusCode(drogon::k201Created);
        callback(response);
        EventsWebSocket::publish(R"({"type":"items.changed"})");
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "note_create_failed", error.what()));
    }
}

void ItemsController::updateNote(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string id) const {
    try {
        const auto body = request->getJsonObject();
        if (!body || !(*body)["content"].isString()) {
            callback(error_response(drogon::k400BadRequest, "invalid_note", "A note content string is required"));
            return;
        }
        auto title = (*body)["title"].asString();
        if (title.empty()) {
            title = "Untitled note";
        }
        const auto item = ssheila::core::Database::active().update_note(
            id, title, (*body)["content"].asString());
        if (!item) {
            callback(error_response(drogon::k404NotFound, "note_not_found", "The note does not exist"));
            return;
        }
        callback(drogon::HttpResponse::newHttpJsonResponse(item_json(*item)));
        EventsWebSocket::publish(R"({"type":"items.changed"})");
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "note_update_failed", error.what()));
    }
}

void ItemsController::upload(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    try {
        drogon::MultiPartParser parser;
        if (parser.parse(request) != 0 || parser.getFiles().empty()) {
            callback(error_response(drogon::k400BadRequest, "invalid_upload", "No multipart files were received"));
            return;
        }

        Json::Value body;
        body["items"] = Json::arrayValue;
        const auto& objectDirectory = ssheila::core::active_storage_layout().objects;
        for (const auto& file : parser.getFiles()) {
            const auto originalName = safe_file_name(file.getFileName());
            const auto extension = std::filesystem::path{originalName}.extension().string();
            const auto objectName = drogon::utils::getUuid() + extension;
            const auto objectPath = objectDirectory / objectName;

            std::ofstream output{objectPath, std::ios::binary | std::ios::trunc};
            if (!output) {
                throw std::runtime_error("Could not create the host-storage object");
            }
            output.write(file.fileData(), static_cast<std::streamsize>(file.fileLength()));
            output.close();
            if (!output) {
                std::filesystem::remove(objectPath);
                throw std::runtime_error("Could not finish writing the uploaded file");
            }

            try {
                const auto item = ssheila::core::Database::active().create_file(
                    originalName,
                    objectPath,
                    mime_for_file(originalName),
                    file.fileLength(),
                    file.getMd5());
                body["items"].append(item_json(item));
            } catch (...) {
                std::filesystem::remove(objectPath);
                throw;
            }
        }

        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k201Created);
        callback(response);
        EventsWebSocket::publish(R"({"type":"items.changed"})");
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "upload_failed", error.what()));
    }
}

void ItemsController::download(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string id) const {
    try {
        const auto item = ssheila::core::Database::active().get_item(id);
        if (!item || item->type != "file" || !item->objectPath ||
            !std::filesystem::is_regular_file(*item->objectPath)) {
            callback(error_response(drogon::k404NotFound, "file_not_found", "The file is unavailable"));
            return;
        }
        callback(drogon::HttpResponse::newFileResponse(
            item->objectPath->string(), item->title, drogon::CT_NONE, "", request));
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "download_failed", error.what()));
    }
}

}  // namespace ssheila::http
