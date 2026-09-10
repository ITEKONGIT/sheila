#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ssheila::core {

class LocalSubnetAccess {
public:
    [[nodiscard]] static LocalSubnetAccess discover();
    [[nodiscard]] static LocalSubnetAccess from_cidrs(const std::vector<std::string>& cidrs);

    [[nodiscard]] bool allows(std::string_view address) const;
    [[nodiscard]] std::vector<std::string> cidrs() const;
    [[nodiscard]] std::vector<std::string> interface_addresses() const;

private:
    struct Network {
        std::uint32_t network;
        std::uint32_t mask;
        std::uint8_t prefix;
        std::string interfaceAddress;
    };

    explicit LocalSubnetAccess(std::vector<Network> networks);
    std::vector<Network> networks_;
};

}  // namespace ssheila::core
