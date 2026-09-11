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
    if (item.type == "file" || item.type == "image" || item.type == "video" ||
        item.type == "audio" || item.type == "document") {
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

    // Images
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".gif") return "image/gif";
    if (extension == ".webp") return "image/webp";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".bmp") return "image/bmp";
    if (extension == ".ico") return "image/x-icon";
    if (extension == ".tiff" || extension == ".tif") return "image/tiff";
    if (extension == ".avif") return "image/avif";

    // Video
    if (extension == ".mp4") return "video/mp4";
    if (extension == ".mkv" || extension == ".webm") return "video/webm";
    if (extension == ".avi") return "video/x-msvideo";
    if (extension == ".mov") return "video/quicktime";
    if (extension == ".flv") return "video/x-flv";
    if (extension == ".wmv") return "video/x-ms-wmv";
    if (extension == ".m4v") return "video/x-m4v";
    if (extension == ".3gp") return "video/3gpp";
    if (extension == ".ts") return "video/mp2t";

    // Audio
    if (extension == ".mp3") return "audio/mpeg";
    if (extension == ".wav") return "audio/wav";
    if (extension == ".ogg") return "audio/ogg";
    if (extension == ".flac") return "audio/flac";
    if (extension == ".aac") return "audio/aac";
    if (extension == ".m4a") return "audio/mp4";
    if (extension == ".wma") return "audio/x-ms-wma";
    if (extension == ".opus") return "audio/opus";

    // Documents
    if (extension == ".pdf") return "application/pdf";
    if (extension == ".doc") return "application/msword";
    if (extension == ".docx") return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (extension == ".xls") return "application/vnd.ms-excel";
    if (extension == ".xlsx") return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    if (extension == ".ppt" || extension == ".pptx") return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    if (extension == ".odt") return "application/vnd.oasis.opendocument.text";
    if (extension == ".ods") return "application/vnd.oasis.opendocument.spreadsheet";
    if (extension == ".csv") return "text/csv";
    if (extension == ".txt" || extension == ".md" || extension == ".log") return "text/plain";
    if (extension == ".html" || extension == ".htm") return "text/html";
    if (extension == ".xml") return "text/xml";
    if (extension == ".json") return "application/json";
    if (extension == ".yaml" || extension == ".yml") return "application/x-yaml";

    // Archives
    if (extension == ".zip") return "application/zip";
    if (extension == ".tar") return "application/x-tar";
    if (extension == ".gz" || extension == ".tgz") return "application/gzip";
    if (extension == ".bz2") return "application/x-bzip2";
    if (extension == ".xz") return "application/x-xz";
    if (extension == ".7z") return "application/x-7z-compressed";
    if (extension == ".rar") return "application/vnd.rar";

    // Executables and packages
    if (extension == ".apk") return "application/vnd.android.package-archive";
    if (extension == ".deb") return "application/x-debian-package";
    if (extension == ".rpm") return "application/x-rpm";
    if (extension == ".exe" || extension == ".msi") return "application/x-msdownload";
    if (extension == ".dmg") return "application/x-apple-diskimage";
    if (extension == ".iso") return "application/x-iso9660-image";

    // Fonts
    if (extension == ".ttf") return "font/ttf";
    if (extension == ".otf") return "font/otf";
    if (extension == ".woff") return "font/woff";
    if (extension == ".woff2") return "font/woff2";

    return "application/octet-stream";
}

std::string item_type_for_mime(const std::string& mime) {
    if (mime.find("image/") == 0) return "image";
    if (mime.find("video/") == 0) return "video";
    if (mime.find("audio/") == 0) return "audio";
    if (mime.find("text/") == 0) return "document";
    if (mime == "application/pdf" ||
        mime.find("application/vnd.") == 0 ||
        mime.find("application/msword") == 0 ||
        mime.find("application/x-debian-package") == 0 ||
        mime.find("application/x-rpm") == 0) {
        return "document";
    }
    return "file";
}

std::string safe_file_name(std::string_view original) {
    auto name = std::filesystem::path{original}.filename().string();
    if (name.empty() || name == "." || name == "..") {
        name = "upload.bin";
    }
    return name;
}

template <typename RequestPtr>
drogon::HttpResponsePtr file_download_response(
    const std::string& path, const std::string& title, const RequestPtr& request) {
    if constexpr (requires {
                      drogon::HttpResponse::newFileResponse(
                          path, title, drogon::CT_NONE, "", request);
                  }) {
        return drogon::HttpResponse::newFileResponse(
            path, title, drogon::CT_NONE, "", request);
    } else {
        // Drogon 1.9.0, shipped by current Debian/Parrot releases, predates
        // request-aware range handling on this factory overload.
        (void)request;
        return drogon::HttpResponse::newFileResponse(path, title, drogon::CT_NONE, "");
    }
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
                const auto mediaType = mime_for_file(originalName);
                const auto itemType = item_type_for_mime(mediaType);
                const auto item = ssheila::core::Database::active().create_file(
                    originalName,
                    objectPath,
                    mediaType,
                    file.fileLength(),
                    file.getMd5(),
                    itemType);
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
        if (!item || (item->type != "file" && item->type != "image" && item->type != "video" &&
                      item->type != "audio" && item->type != "document") ||
            !item->objectPath ||
            !std::filesystem::is_regular_file(*item->objectPath)) {
            callback(error_response(drogon::k404NotFound, "file_not_found", "The file is unavailable"));
            return;
        }
        callback(file_download_response(item->objectPath->string(), item->title, request));
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "download_failed", error.what()));
    }
}

void ItemsController::removeItem(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string id) const {
    try {
        if (id.empty()) {
            callback(error_response(drogon::k400BadRequest, "invalid_id", "Item ID is required"));
            return;
        }
        if (ssheila::core::Database::active().delete_item(id)) {
            auto response = drogon::HttpResponse::newHttpJsonResponse(Json::Value{});
            response->setStatusCode(drogon::k204NoContent);
            callback(response);
            EventsWebSocket::publish(R"({"type":"items.changed"})");
        } else {
            callback(error_response(drogon::k404NotFound, "item_not_found", "The item does not exist"));
        }
    } catch (const std::exception& error) {
        callback(error_response(drogon::k500InternalServerError, "delete_failed", error.what()));
    }
}

}  // namespace ssheila::http
