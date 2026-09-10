#include "ssheila/http/system_controller.hpp"

#include "ssheila/core/storage_layout.hpp"

#include <json/json.h>

#include <filesystem>

namespace ssheila::http {

void SystemController::get(const drogon::HttpRequestPtr&,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    const auto& layout = ssheila::core::active_storage_layout();
    const auto space = std::filesystem::space(layout.root);

    Json::Value body;
#ifdef _WIN32
    body["platform"] = "windows";
#elif __APPLE__
    body["platform"] = "macos";
#else
    body["platform"] = "linux";
#endif
    body["storage"]["capacityBytes"] = Json::UInt64(space.capacity);
    body["storage"]["availableBytes"] = Json::UInt64(space.available);
    body["storage"]["usedBytes"] = Json::UInt64(space.capacity - space.free);
    callback(drogon::HttpResponse::newHttpJsonResponse(body));
}

}  // namespace ssheila::http
