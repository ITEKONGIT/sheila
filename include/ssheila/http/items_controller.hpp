#pragma once

#include <drogon/HttpController.h>

namespace ssheila::http {

class ItemsController : public drogon::HttpController<ItemsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ItemsController::list, "/api/v1/items", drogon::Get);
    ADD_METHOD_TO(ItemsController::createNote, "/api/v1/notes", drogon::Post);
    ADD_METHOD_TO(ItemsController::updateNote, "/api/v1/notes/{1}", drogon::Put);
    ADD_METHOD_TO(ItemsController::upload, "/api/v1/files", drogon::Post);
    ADD_METHOD_TO(ItemsController::download, "/api/v1/files/{1}", drogon::Get);
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr& request,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    void createNote(const drogon::HttpRequestPtr& request,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    void updateNote(const drogon::HttpRequestPtr& request,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    std::string id) const;
    void upload(const drogon::HttpRequestPtr& request,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    void download(const drogon::HttpRequestPtr& request,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  std::string id) const;
};

}  // namespace ssheila::http
