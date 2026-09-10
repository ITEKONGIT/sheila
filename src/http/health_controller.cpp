#include "ssheila/http/health_controller.hpp"

#include <json/json.h>

namespace ssheila::http {

void HealthController::get(const drogon::HttpRequestPtr&,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    Json::Value body;
    body["service"] = "sSheila";
    body["status"] = "ok";
    body["version"] = "0.1.0";
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
}

}  // namespace ssheila::http
