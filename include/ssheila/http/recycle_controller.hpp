#pragma once

#include <drogon/HttpController.h>

namespace ssheila::http {

class RecycleController : public drogon::HttpController<RecycleController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(RecycleController::list, "/api/v1/recycle-bin", drogon::Get);
    ADD_METHOD_TO(RecycleController::restoreItem, "/api/v1/recycle-bin/items/{1}/restore", drogon::Post);
    ADD_METHOD_TO(RecycleController::purgeItem, "/api/v1/recycle-bin/items/{1}", drogon::Delete);
    ADD_METHOD_TO(RecycleController::restoreTask, "/api/v1/recycle-bin/tasks/{1}/restore", drogon::Post);
    ADD_METHOD_TO(RecycleController::purgeTask, "/api/v1/recycle-bin/tasks/{1}", drogon::Delete);
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr& request,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    void restoreItem(const drogon::HttpRequestPtr& request,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     std::string id) const;
    void purgeItem(const drogon::HttpRequestPtr& request,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   std::string id) const;
    void restoreTask(const drogon::HttpRequestPtr& request,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     std::string id) const;
    void purgeTask(const drogon::HttpRequestPtr& request,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   std::string id) const;
};

}  // namespace ssheila::http
