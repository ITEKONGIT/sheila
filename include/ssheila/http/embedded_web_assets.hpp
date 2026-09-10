#pragma once

#include <optional>
#include <string_view>

namespace ssheila::http {

struct EmbeddedWebAsset {
    std::string_view body;
    std::string_view contentType;
};

std::optional<EmbeddedWebAsset> find_embedded_web_asset(std::string_view path);

}  // namespace ssheila::http

