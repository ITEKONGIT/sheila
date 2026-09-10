#pragma once

#include <drogon/HttpController.h>

namespace ssheila::http {

class SystemController : public drogon::HttpController<SystemController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SystemController::get, "/api/v1/system", drogon::Get);
    METHOD_LIST_END

    void get(const drogon::HttpRequestPtr& request,
             std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
};

}  // namespace ssheila::http
