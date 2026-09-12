#include "ssheila/core/mdns_responder.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ssheila::core {
namespace {

constexpr std::string_view multicastAddress{"224.0.0.251"};
constexpr std::uint32_t recordTtl = 120;

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket invalidSocket = INVALID_SOCKET;
using SocketLength = int;
#else
using Socket = int;
constexpr Socket invalidSocket = -1;
using SocketLength = socklen_t;
#endif

void close_socket(Socket socket) {
    if (socket == invalidSocket) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

std::string socket_error() {
#ifdef _WIN32
    return "Winsock error " + std::to_string(WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

bool set_socket_option(Socket socket, int level, int option, const void* value, SocketLength length) {
#ifdef _WIN32
    return setsockopt(socket, level, option, static_cast<const char*>(value), length) == 0;
#else
    return setsockopt(socket, level, option, value, length) == 0;
#endif
}

bool parse_ipv4(std::string_view text, in_addr& result) {
    const std::string value{text};
    return inet_pton(AF_INET, value.c_str(), &result) == 1;
}

std::string normalized_hostname(std::string_view hostname) {
    std::string result;
    result.reserve(hostname.size());
    for (const auto character : hostname) {
        if (character == '.') {
            result.push_back(character);
        } else {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        }
    }
    if (!result.empty() && result.back() == '.') {
        result.pop_back();
    }
    return result;
}

void append_u16(std::vector<unsigned char>& packet, std::uint16_t value) {
    packet.push_back(static_cast<unsigned char>((value >> 8u) & 0xFFu));
    packet.push_back(static_cast<unsigned char>(value & 0xFFu));
}

void append_u32(std::vector<unsigned char>& packet, std::uint32_t value) {
    packet.push_back(static_cast<unsigned char>((value >> 24u) & 0xFFu));
    packet.push_back(static_cast<unsigned char>((value >> 16u) & 0xFFu));
    packet.push_back(static_cast<unsigned char>((value >> 8u) & 0xFFu));
    packet.push_back(static_cast<unsigned char>(value & 0xFFu));
}

void append_dns_name(std::vector<unsigned char>& packet, std::string_view hostname) {
    std::size_t labelStart = 0;
    while (labelStart < hostname.size()) {
        const auto labelEnd = hostname.find('.', labelStart);
        const auto labelLength = labelEnd == std::string_view::npos
            ? hostname.size() - labelStart
            : labelEnd - labelStart;
        if (labelLength == 0 || labelLength > 63) {
            throw std::invalid_argument("Invalid mDNS hostname label");
        }
        packet.push_back(static_cast<unsigned char>(labelLength));
        packet.insert(packet.end(), hostname.begin() + static_cast<std::ptrdiff_t>(labelStart),
                      hostname.begin() + static_cast<std::ptrdiff_t>(labelStart + labelLength));
        if (labelEnd == std::string_view::npos) {
            break;
        }
        labelStart = labelEnd + 1;
    }
    packet.push_back(0);
}

bool read_dns_name(const std::vector<unsigned char>& packet, std::size_t& offset, std::string& result) {
    result.clear();
    std::size_t cursor = offset;
    bool jumped = false;
    std::size_t jumps = 0;

    while (cursor < packet.size()) {
        const auto length = packet[cursor++];
        if (length == 0) {
            if (!jumped) {
                offset = cursor;
            }
            return true;
        }
        if ((length & 0xC0u) == 0xC0u) {
            if (cursor >= packet.size()) {
                return false;
            }
            const auto pointer = static_cast<std::size_t>((length & 0x3Fu) << 8u) | packet[cursor++];
            if (pointer >= packet.size() || ++jumps > 16) {
                return false;
            }
            if (!jumped) {
                offset = cursor;
                jumped = true;
            }
            cursor = pointer;
            continue;
        }
        if (length > 63 || cursor + length > packet.size()) {
            return false;
        }
        if (!result.empty()) {
            result.push_back('.');
        }
        for (std::size_t index = 0; index < length; ++index) {
            result.push_back(static_cast<char>(std::tolower(
                static_cast<unsigned char>(packet[cursor + index]))));
        }
        cursor += length;
    }
    return false;
}

std::vector<unsigned char> make_response(std::string_view hostname,
                                         const std::vector<in_addr>& addresses,
                                         std::uint16_t transactionId = 0) {
    std::vector<unsigned char> packet;
    // Interface discovery produces only a small number of addresses in
    // practice. A fixed initial capacity also avoids size arithmetic based on
    // externally-derived vector lengths in the packet builder.
    packet.reserve(256);
    append_u16(packet, transactionId);
    append_u16(packet, 0x8400);  // response, authoritative answer.
    append_u16(packet, 0);       // no questions in an unsolicited response.
    append_u16(packet, static_cast<std::uint16_t>(addresses.size()));
    append_u16(packet, 0);
    append_u16(packet, 0);

    for (const auto& address : addresses) {
        append_dns_name(packet, hostname);
        append_u16(packet, 1);             // A
        append_u16(packet, 1);             // IN
        append_u32(packet, recordTtl);
        append_u16(packet, 4);
        const auto* bytes = reinterpret_cast<const unsigned char*>(&address.s_addr);
        packet.insert(packet.end(), bytes, bytes + 4);
    }
    return packet;
}

bool query_matches(const std::vector<unsigned char>& packet, std::string_view hostname) {
    if (packet.size() < 12) {
        return false;
    }

    const auto questionCount = static_cast<std::uint16_t>(packet[4] << 8u) | packet[5];
    if (questionCount == 0) {
        return false;
    }

    std::size_t offset = 12;
    for (std::uint16_t question = 0; question < questionCount; ++question) {
        std::string name;
        if (!read_dns_name(packet, offset, name) || offset + 4 > packet.size()) {
            return false;
        }
        const auto type = static_cast<std::uint16_t>(packet[offset] << 8u) | packet[offset + 1];
        const auto queryClass = static_cast<std::uint16_t>(packet[offset + 2] << 8u) | packet[offset + 3];
        offset += 4;

        // Answer A and ANY queries. AAAA is intentionally omitted because
        // subnet discovery and HTTP access are currently IPv4-only.
        if (name == hostname && (type == 1 || type == 255) &&
            ((queryClass & 0x7FFFu) == 1 || (queryClass & 0x7FFFu) == 255)) {
            return true;
        }
    }
    return false;
}

}  // namespace

struct MdnsRuntime {
    Socket socket{invalidSocket};
    std::atomic<bool> stopping{false};
#ifdef _WIN32
    bool winsockStarted{false};
#endif
};

MdnsResponder::MdnsResponder(std::vector<std::string> interfaceAddresses)
    : interfaceAddresses_(std::move(interfaceAddresses)) {}

MdnsResponder::~MdnsResponder() {
    stop();
}

bool MdnsResponder::start(std::string& error) {
    if (started_) {
        return true;
    }
    if (interfaceAddresses_.empty()) {
        error = "No active IPv4 interface is available for mDNS";
        return false;
    }

    auto runtime = std::make_shared<MdnsRuntime>();
#ifdef _WIN32
    WSADATA winsockData{};
    const auto winsockError = WSAStartup(MAKEWORD(2, 2), &winsockData);
    if (winsockError != 0) {
        error = "Could not initialize Winsock for mDNS: error " + std::to_string(winsockError);
        return false;
    }
    runtime->winsockStarted = true;
#endif

    runtime->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (runtime->socket == invalidSocket) {
        error = "Could not create the mDNS socket: " + socket_error();
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    int reuseAddress = 1;
    if (!set_socket_option(runtime->socket, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress))) {
        error = "Could not configure the mDNS socket: " + socket_error();
        close_socket(runtime->socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    sockaddr_in bindAddress{};
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = htons(kMdnsPort);
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(runtime->socket, reinterpret_cast<const sockaddr*>(&bindAddress), sizeof(bindAddress)) != 0) {
        error = "Could not bind UDP port 5353 for mDNS: " + socket_error();
        close_socket(runtime->socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    in_addr multicast{};
    if (!parse_ipv4(multicastAddress, multicast)) {
        error = "Could not parse the mDNS multicast address";
        close_socket(runtime->socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    unsigned char multicastTtl = 255;
    if (!set_socket_option(runtime->socket, IPPROTO_IP, IP_MULTICAST_TTL,
                           &multicastTtl, sizeof(multicastTtl))) {
        error = "Could not configure the mDNS multicast TTL: " + socket_error();
        close_socket(runtime->socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    bool joinedInterface = false;
    for (const auto& interfaceAddress : interfaceAddresses_) {
        in_addr interface{};
        if (!parse_ipv4(interfaceAddress, interface)) {
            continue;
        }
        ip_mreq membership{};
        membership.imr_multiaddr = multicast;
        membership.imr_interface = interface;
        if (set_socket_option(runtime->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                              &membership, sizeof(membership))) {
            joinedInterface = true;
        }
    }

    if (!joinedInterface) {
        ip_mreq membership{};
        membership.imr_multiaddr = multicast;
        membership.imr_interface.s_addr = htonl(INADDR_ANY);
        if (!set_socket_option(runtime->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                               &membership, sizeof(membership))) {
            error = "Could not join the mDNS multicast group: " + socket_error();
            close_socket(runtime->socket);
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }
    }

    runtime_ = runtime;
    started_ = true;
    thread_ = std::thread([runtime, addresses = interfaceAddresses_] {
        const auto hostname = normalized_hostname(kMdnsHostname);
        std::vector<in_addr> parsedAddresses;
        for (const auto& value : addresses) {
            in_addr address{};
            if (parse_ipv4(value, address)) {
                parsedAddresses.push_back(address);
            }
        }
        if (parsedAddresses.empty()) {
            close_socket(runtime->socket);
#ifdef _WIN32
            if (runtime->winsockStarted) {
                WSACleanup();
            }
#endif
            return;
        }

        const auto response = make_response(hostname, parsedAddresses);
        sockaddr_in multicastDestination{};
        multicastDestination.sin_family = AF_INET;
        multicastDestination.sin_port = htons(kMdnsPort);
        parse_ipv4(multicastAddress, multicastDestination.sin_addr);

        auto announce = [&] {
            for (const auto& address : parsedAddresses) {
                set_socket_option(runtime->socket, IPPROTO_IP, IP_MULTICAST_IF,
                                  &address, sizeof(address));
                sendto(runtime->socket, reinterpret_cast<const char*>(response.data()),
                       static_cast<int>(response.size()), 0,
                       reinterpret_cast<const sockaddr*>(&multicastDestination),
                       sizeof(multicastDestination));
            }
        };

        auto nextAnnouncement = std::chrono::steady_clock::now();
        int announcementsSent = 0;
        std::array<unsigned char, 1500> buffer{};
        while (!runtime->stopping.load()) {
            const auto now = std::chrono::steady_clock::now();
            if (announcementsSent < 3 && now >= nextAnnouncement) {
                announce();
                ++announcementsSent;
                nextAnnouncement = now + std::chrono::seconds(1);
            }

            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(runtime->socket, &readSet);
            timeval timeout{0, 250000};
#ifdef _WIN32
            const auto ready = select(0, &readSet, nullptr, nullptr, &timeout);
#else
            const auto ready = select(runtime->socket + 1, &readSet, nullptr, nullptr, &timeout);
#endif
            if (ready <= 0 || !FD_ISSET(runtime->socket, &readSet)) {
                continue;
            }

            sockaddr_storage source{};
            SocketLength sourceLength = sizeof(source);
            const auto received = recvfrom(runtime->socket, reinterpret_cast<char*>(buffer.data()),
                                           static_cast<int>(buffer.size()), 0,
                                           reinterpret_cast<sockaddr*>(&source), &sourceLength);
            if (received <= 0) {
                continue;
            }
            std::vector<unsigned char> query(buffer.begin(), buffer.begin() + received);
            if (!query_matches(query, hostname)) {
                continue;
            }

            // Multicast responses are visible to every client on each
            // interface. A unicast-query source gets a direct response.
            const auto* sourceAddress = reinterpret_cast<const sockaddr_in*>(&source);
            const bool unicastQuery = sourceAddress->sin_port != htons(kMdnsPort);
            if (unicastQuery) {
                const auto transactionId = static_cast<std::uint16_t>(query[0] << 8u) | query[1];
                const auto queryResponse = transactionId == 0
                    ? response
                    : make_response(hostname, parsedAddresses, transactionId);
                sendto(runtime->socket, reinterpret_cast<const char*>(queryResponse.data()),
                       static_cast<int>(queryResponse.size()), 0,
                       reinterpret_cast<const sockaddr*>(&source), sourceLength);
                continue;
            }
            announce();
        }

        close_socket(runtime->socket);
#ifdef _WIN32
        if (runtime->winsockStarted) {
            WSACleanup();
        }
#endif
    });
    return true;
}

bool MdnsResponder::reconfigure(std::vector<std::string> interfaceAddresses,
                                std::string& error) {
    if (interfaceAddresses == interfaceAddresses_) {
        return true;
    }

    const auto previousAddresses = interfaceAddresses_;
    stop();
    interfaceAddresses_ = std::move(interfaceAddresses);
    if (start(error)) {
        return true;
    }

    // Keep the existing advertisement available if a transient network
    // change made the new interface set unusable.
    interfaceAddresses_ = previousAddresses;
    std::string restoreError;
    if (!start(restoreError) && !restoreError.empty()) {
        error += "; could not restore the previous mDNS advertisement: " + restoreError;
    }
    return false;
}

void MdnsResponder::stop() {
    if (!started_) {
        return;
    }
    runtime_->stopping.store(true);
    thread_.join();
    runtime_.reset();
    started_ = false;
}

}  // namespace ssheila::core
