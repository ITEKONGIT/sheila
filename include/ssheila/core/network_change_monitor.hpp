#pragma once

#include <functional>
#include <memory>
#include <string>

namespace ssheila::core {

class NetworkChangeMonitor final {
public:
    using Callback = std::function<void()>;

    explicit NetworkChangeMonitor(Callback callback);
    ~NetworkChangeMonitor();

    NetworkChangeMonitor(const NetworkChangeMonitor&) = delete;
    NetworkChangeMonitor& operator=(const NetworkChangeMonitor&) = delete;

    // Uses the platform's network-change notification facility. A caller can
    // retain a slow safety rescan when this is unavailable.
    [[nodiscard]] bool start(std::string& error);
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ssheila::core
