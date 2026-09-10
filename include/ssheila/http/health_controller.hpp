#pragma once

#include <drogon/HttpController.h>

namespace ssheila::http {

class HealthController : public drogon::HttpController<HealthController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HealthController::get, "/api/v1/health", drogon::Get);
    METHOD_LIST_END

    void get(const drogon::HttpRequestPtr& request,
             std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
};

}  // namespace ssheila::http
