#include "ssheila/http/embedded_web_controller.hpp"

#include "ssheila/http/embedded_web_assets.hpp"

#include <stdexcept>
#include <string>

namespace ssheila::http {

void EmbeddedWebController::index(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    send(request->path() == "/index.html" ? "/index.html" : "/", std::move(callback));
}

void EmbeddedWebController::recycleBin(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    send("/recycle-bin", std::move(callback));
}

void EmbeddedWebController::recycleBinScript(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    send("/recycle-bin.js", std::move(callback));
}

void EmbeddedWebController::styles(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    send("/styles.css", std::move(callback));
}

void EmbeddedWebController::application(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    send("/app.js", std::move(callback));
}

void EmbeddedWebController::manifest(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    send("/manifest.webmanifest", std::move(callback));
}

void EmbeddedWebController::favicon(
    const drogon::HttpRequestPtr&,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    send("/favicon.svg", std::move(callback));
}

void EmbeddedWebController::send(
    std::string_view path,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    const auto asset = find_embedded_web_asset(path);
    if (!asset) {
        throw std::runtime_error("Embedded web asset is missing");
    }

    auto response = drogon::HttpResponse::newHttpResponse();
    response->setBody(std::string{asset->body});
    response->addHeader("Content-Type", std::string{asset->contentType});
    response->addHeader("Cache-Control", "no-cache");
    callback(response);
}

}  // namespace ssheila::http
