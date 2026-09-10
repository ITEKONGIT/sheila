#include "ssheila/core/local_subnet_access.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <stdexcept>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace ssheila::core {
namespace {

std::uint32_t prefix_mask(std::uint8_t prefix) {
    if (prefix == 0) {
        return 0;
    }
    return 0xFFFFFFFFu << (32u - prefix);
}

#ifndef _WIN32
std::uint8_t mask_prefix(std::uint32_t mask) {
    std::uint8_t prefix = 0;
    bool sawZero = false;
    for (int bit = 31; bit >= 0; --bit) {
        const bool set = (mask & (1u << bit)) != 0;
        if (set && sawZero) {
            return 0;
        }
        if (set) {
            ++prefix;
        } else {
            sawZero = true;
        }
    }
    return prefix;
}
#endif

bool parse_ipv4(std::string_view text, std::uint32_t& result) {
    const std::string value{text};
    in_addr address{};
    if (inet_pton(AF_INET, value.c_str(), &address) != 1) {
        return false;
    }
    result = ntohl(address.s_addr);
    return true;
}

std::string format_ipv4(std::uint32_t hostOrderAddress) {
    in_addr address{};
    address.s_addr = htonl(hostOrderAddress);
    std::array<char, INET_ADDRSTRLEN> buffer{};
    if (inet_ntop(AF_INET, &address, buffer.data(), buffer.size()) == nullptr) {
        throw std::runtime_error("Could not format an IPv4 address");
    }
    return buffer.data();
}

bool is_loopback(std::uint32_t address) {
    return (address & 0xFF000000u) == 0x7F000000u;
}

template <typename NetworkType>
void deduplicate(std::vector<NetworkType>& networks) {
    std::sort(networks.begin(), networks.end(), [](const auto& left, const auto& right) {
        if (left.network != right.network) {
            return left.network < right.network;
        }
        if (left.mask != right.mask) {
            return left.mask < right.mask;
        }
        return left.interfaceAddress < right.interfaceAddress;
    });
    networks.erase(std::unique(networks.begin(), networks.end(), [](const auto& left, const auto& right) {
        return left.network == right.network && left.mask == right.mask &&
               left.interfaceAddress == right.interfaceAddress;
    }), networks.end());
}

}  // namespace

LocalSubnetAccess::LocalSubnetAccess(std::vector<Network> networks)
    : networks_(std::move(networks)) {
    deduplicate(networks_);
}

LocalSubnetAccess LocalSubnetAccess::from_cidrs(const std::vector<std::string>& cidrs) {
    std::vector<Network> networks;
    networks.reserve(cidrs.size());

    for (const auto& cidr : cidrs) {
        const auto separator = cidr.find('/');
        if (separator == std::string::npos) {
            throw std::invalid_argument("CIDR is missing a prefix: " + cidr);
        }

        std::uint32_t address = 0;
        if (!parse_ipv4(std::string_view{cidr}.substr(0, separator), address)) {
            throw std::invalid_argument("Invalid IPv4 address in CIDR: " + cidr);
        }

        unsigned int prefixValue = 0;
        const auto prefixText = std::string_view{cidr}.substr(separator + 1);
        const auto [end, error] = std::from_chars(
            prefixText.data(), prefixText.data() + prefixText.size(), prefixValue);
        if (error != std::errc{} || end != prefixText.data() + prefixText.size() || prefixValue > 32) {
            throw std::invalid_argument("Invalid IPv4 prefix in CIDR: " + cidr);
        }

        const auto prefix = static_cast<std::uint8_t>(prefixValue);
        const auto mask = prefix_mask(prefix);
        networks.push_back(Network{
            .network = address & mask,
            .mask = mask,
            .prefix = prefix,
            .interfaceAddress = format_ipv4(address),
        });
    }

    return LocalSubnetAccess{std::move(networks)};
}

LocalSubnetAccess LocalSubnetAccess::discover() {
    std::vector<Network> networks;

#ifdef _WIN32
    ULONG bufferSize = 16 * 1024;
    std::vector<unsigned char> buffer(bufferSize);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    ULONG status = GetAdaptersAddresses(
        AF_INET,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr,
        adapters,
        &bufferSize);

    if (status == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufferSize);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        status = GetAdaptersAddresses(
            AF_INET,
            GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            nullptr,
            adapters,
            &bufferSize);
    }
    if (status != NO_ERROR) {
        throw std::runtime_error("Could not enumerate Windows network adapters");
    }

    for (auto* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) {
            continue;
        }
        for (auto* unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr == nullptr ||
                unicast->Address.lpSockaddr->sa_family != AF_INET ||
                unicast->OnLinkPrefixLength == 0 ||
                unicast->OnLinkPrefixLength > 32) {
                continue;
            }
            const auto* socketAddress = reinterpret_cast<const sockaddr_in*>(unicast->Address.lpSockaddr);
            const auto address = ntohl(socketAddress->sin_addr.s_addr);
            if (is_loopback(address)) {
                continue;
            }
            const auto prefix = static_cast<std::uint8_t>(unicast->OnLinkPrefixLength);
            const auto mask = prefix_mask(prefix);
            networks.push_back(Network{
                .network = address & mask,
                .mask = mask,
                .prefix = prefix,
                .interfaceAddress = format_ipv4(address),
            });
        }
    }
#else
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0) {
        throw std::runtime_error("Could not enumerate Linux network interfaces");
    }

    for (auto* interface = interfaces; interface != nullptr; interface = interface->ifa_next) {
        if (interface->ifa_addr == nullptr || interface->ifa_netmask == nullptr ||
            interface->ifa_addr->sa_family != AF_INET ||
            (interface->ifa_flags & IFF_UP) == 0) {
            continue;
        }

        const auto* socketAddress = reinterpret_cast<const sockaddr_in*>(interface->ifa_addr);
        const auto* socketMask = reinterpret_cast<const sockaddr_in*>(interface->ifa_netmask);
        const auto address = ntohl(socketAddress->sin_addr.s_addr);
        const auto mask = ntohl(socketMask->sin_addr.s_addr);
        const auto prefix = mask_prefix(mask);
        if (prefix == 0 || is_loopback(address)) {
            continue;
        }
        networks.push_back(Network{
            .network = address & mask,
            .mask = mask,
            .prefix = prefix,
            .interfaceAddress = format_ipv4(address),
        });
    }
    freeifaddrs(interfaces);
#endif

    return LocalSubnetAccess{std::move(networks)};
}

bool LocalSubnetAccess::allows(std::string_view addressText) const {
    std::uint32_t address = 0;
    if (!parse_ipv4(addressText, address)) {
        return false;
    }
    if (is_loopback(address)) {
        return true;
    }
    return std::any_of(networks_.begin(), networks_.end(), [address](const auto& network) {
        return (address & network.mask) == network.network;
    });
}

std::vector<std::string> LocalSubnetAccess::cidrs() const {
    std::vector<std::string> result;
    result.reserve(networks_.size());
    for (const auto& network : networks_) {
        result.push_back(format_ipv4(network.network) + "/" + std::to_string(network.prefix));
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<std::string> LocalSubnetAccess::interface_addresses() const {
    std::vector<std::string> result;
    result.reserve(networks_.size());
    for (const auto& network : networks_) {
        result.push_back(network.interfaceAddress);
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

}  // namespace ssheila::core
