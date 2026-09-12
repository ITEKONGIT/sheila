#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace ssheila::core {

struct MdnsRuntime;

inline constexpr const char* kMdnsHostname = "sheila.local";
inline constexpr std::uint16_t kMdnsPort = 5353;

class MdnsResponder final {
public:
    explicit MdnsResponder(std::vector<std::string> interfaceAddresses);
    ~MdnsResponder();

    MdnsResponder(const MdnsResponder&) = delete;
    MdnsResponder& operator=(const MdnsResponder&) = delete;

    // Returns false when the host cannot bind or join the mDNS socket. mDNS
    // is an enhancement to the HTTP service, so callers can continue serving
    // by IP and report the returned error instead of aborting startup.
    [[nodiscard]] bool start(std::string& error);
    [[nodiscard]] bool reconfigure(std::vector<std::string> interfaceAddresses,
                                   std::string& error);
    void stop();

private:
    std::vector<std::string> interfaceAddresses_;
    std::shared_ptr<MdnsRuntime> runtime_;
    std::thread thread_;
    bool started_{false};
};

}  // namespace ssheila::core
