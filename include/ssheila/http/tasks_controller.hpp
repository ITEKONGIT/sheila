#pragma once

#include <drogon/HttpController.h>

namespace ssheila::http {

class TasksController : public drogon::HttpController<TasksController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(TasksController::list, "/api/v1/tasks", drogon::Get);
    ADD_METHOD_TO(TasksController::create, "/api/v1/tasks", drogon::Post);
    ADD_METHOD_TO(TasksController::update, "/api/v1/tasks/{1}", drogon::Put);
    ADD_METHOD_TO(TasksController::complete, "/api/v1/tasks/{1}/complete", drogon::Post);
    ADD_METHOD_TO(TasksController::trash, "/api/v1/tasks/{1}", drogon::Delete);
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr& request,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    void create(const drogon::HttpRequestPtr& request,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    void update(const drogon::HttpRequestPtr& request,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                std::string id) const;
    void complete(const drogon::HttpRequestPtr& request,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  std::string id) const;
    void trash(const drogon::HttpRequestPtr& request,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback,
               std::string id) const;
};

}  // namespace ssheila::http
