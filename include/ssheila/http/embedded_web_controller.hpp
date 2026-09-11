#pragma once

#include <drogon/HttpController.h>

namespace ssheila::http {

class EmbeddedWebController : public drogon::HttpController<EmbeddedWebController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(EmbeddedWebController::index, "/", drogon::Get);
    ADD_METHOD_TO(EmbeddedWebController::index, "/index.html", drogon::Get);
    ADD_METHOD_TO(EmbeddedWebController::styles, "/styles.css", drogon::Get);
    ADD_METHOD_TO(EmbeddedWebController::application, "/app.js", drogon::Get);
    ADD_METHOD_TO(EmbeddedWebController::manifest, "/manifest.webmanifest", drogon::Get);
    ADD_METHOD_TO(EmbeddedWebController::favicon, "/favicon.svg", drogon::Get);
    METHOD_LIST_END

    void index(const drogon::HttpRequestPtr& request,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    void styles(const drogon::HttpRequestPtr& request,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    void application(const drogon::HttpRequestPtr& request,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    void manifest(const drogon::HttpRequestPtr& request,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    void favicon(const drogon::HttpRequestPtr& request,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

private:
    static void send(std::string_view path,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

}  // namespace ssheila::http
