#include "ssheila/core/database.hpp"
#include "ssheila/core/local_subnet_access.hpp"
#include "ssheila/core/mdns_responder.hpp"
#include "ssheila/core/network_change_monitor.hpp"
#include "ssheila/core/storage_layout.hpp"
#include "ssheila/http/events_websocket.hpp"

#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct Options {
    std::string address{"0.0.0.0"};
    std::uint16_t port{18877};
    std::filesystem::path dataRoot{ssheila::core::default_data_root()};
};

std::uint64_t max_body_size_bytes() {
    constexpr std::uint64_t defaultMiB = 512;
    constexpr std::uint64_t maximumMiB = 4096;
    const auto* env = std::getenv("SSHEILA_MAX_BODY_SIZE_MB");
    if (env == nullptr || *env == '\0') {
        return defaultMiB * 1024ULL * 1024ULL;
    }

    try {
        const std::string_view raw{env};
        std::size_t consumed = 0;
        const auto value = std::stoull(std::string{raw}, &consumed);
        if (consumed == raw.size() && value > 0 && value <= maximumMiB) {
            return value * 1024ULL * 1024ULL;
        }
    } catch (const std::exception&) {
        // Fall through to the safe default. A bad environment override should
        // not prevent the host service from starting.
    }

    std::cerr << "Ignoring invalid SSHEILA_MAX_BODY_SIZE_MB; using " << defaultMiB
              << " MiB\n";
    return defaultMiB * 1024ULL * 1024ULL;
}

Options parse_options(int argc, char* argv[]) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto require_value = [&]() -> std::string_view {
            if (++index >= argc) {
                throw std::invalid_argument("Missing value after " + std::string{argument});
            }
            return argv[index];
        };

        if (argument == "--address") {
            options.address = require_value();
        } else if (argument == "--port") {
            const auto value = std::stoi(std::string{require_value()});
            if (value < 1 || value > 65535) {
                throw std::invalid_argument("Port must be between 1 and 65535");
            }
            options.port = static_cast<std::uint16_t>(value);
        } else if (argument == "--data-dir") {
            options.dataRoot = require_value();
        } else if (argument == "--help") {
            std::cout << "sSheila options:\n"
                      << "  --address <address>  Bind address (default: 0.0.0.0)\n"
                      << "  --port <port>        HTTP port (default: 18877)\n"
                      << "  --data-dir <path>    Host storage root\n";
            std::exit(0);
        } else {
            throw std::invalid_argument("Unknown option: " + std::string{argument});
        }
    }
    return options;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = parse_options(argc, argv);
        const auto layout = ssheila::core::make_storage_layout(std::filesystem::absolute(options.dataRoot));
        ssheila::core::create_storage_layout(layout);
        ssheila::core::set_active_storage_layout(layout);

        ssheila::core::Database database{layout.database};
        database.migrate();
        ssheila::core::Database::set_active(database);

        const auto subnetAccess = std::make_shared<ssheila::core::LocalSubnetAccess>(
            ssheila::core::LocalSubnetAccess::discover());
        const auto subnetAccessMutex = std::make_shared<std::mutex>();

        ssheila::core::MdnsResponder mdnsResponder{subnetAccess->interface_addresses()};
        std::string mdnsError;
        if (!mdnsResponder.start(mdnsError)) {
            std::cerr << "Could not advertise " << ssheila::core::kMdnsHostname
                      << ": " << mdnsError << '\n';
        }

        const auto networkRefreshMutex = std::make_shared<std::mutex>();

        std::cout << "sSheila 0.1.0\n"
                  << "Storage: " << layout.root << '\n'
                  << "Local: http://127.0.0.1:" << options.port << '\n';
        std::cout << "Hostname: http://" << ssheila::core::kMdnsHostname << ':'
                  << options.port << '\n';
        for (const auto& address : subnetAccess->interface_addresses()) {
            std::cout << "Network: http://" << address << ':' << options.port << '\n';
        }
        std::cout << "Allowed subnets:";
        for (const auto& cidr : subnetAccess->cidrs()) {
            std::cout << ' ' << cidr;
        }
        std::cout << '\n' << std::flush;

        drogon::app()
            .addListener(options.address, options.port)
            .setUploadPath(layout.uploads.string())
            .setClientMaxBodySize(max_body_size_bytes())
            .setThreadNum(0)
            .setLogLevel(trantor::Logger::kWarn);

        drogon::app().registerPreRoutingAdvice(
            [subnetAccess, subnetAccessMutex](const drogon::HttpRequestPtr& request,
                           drogon::AdviceCallback&& reject,
                           drogon::AdviceChainCallback&& continueRequest) {
                const std::lock_guard lock{*subnetAccessMutex};
                if (subnetAccess->allows(request->peerAddr().toIp())) {
                    continueRequest();
                    return;
                }

                Json::Value body;
                body["error"] = "subnet_access_denied";
                body["message"] = "sSheila only accepts devices on the host subnet";
                auto response = drogon::HttpResponse::newHttpJsonResponse(body);
                response->setStatusCode(drogon::k403Forbidden);
                reject(response);
            });

        // Network interfaces can change while the host is running, for
        // example when a laptop moves to another Wi-Fi network. Refreshing
        // both the access gate and mDNS keeps the public hostname stable while
        // making it resolve only on the host's currently attached networks.
        const auto refreshNetwork = [subnetAccess, subnetAccessMutex,
                                     networkRefreshMutex, &mdnsResponder] {
            const std::lock_guard refreshLock{*networkRefreshMutex};
            try {
                const auto latest = ssheila::core::LocalSubnetAccess::discover();
                const auto latestAddresses = latest.interface_addresses();
                const auto latestCidrs = latest.cidrs();
                bool changed = false;
                {
                    const std::lock_guard lock{*subnetAccessMutex};
                    changed = subnetAccess->interface_addresses() != latestAddresses ||
                              subnetAccess->cidrs() != latestCidrs;
                    if (changed) {
                        *subnetAccess = latest;
                    }
                }
                if (changed) {
                    std::string error;
                    if (!mdnsResponder.reconfigure(latestAddresses, error)) {
                        std::cerr << "Could not refresh " << ssheila::core::kMdnsHostname
                                  << ": " << error << '\n';
                    }
                    std::cout << "Network configuration refreshed for "
                              << ssheila::core::kMdnsHostname << '\n' << std::flush;
                }
            } catch (const std::exception& error) {
                std::cerr << "Network discovery refresh failed: " << error.what() << '\n';
            }
        };

        ssheila::core::NetworkChangeMonitor networkChangeMonitor{refreshNetwork};
        std::string networkMonitorError;
        if (!networkChangeMonitor.start(networkMonitorError)) {
            std::cerr << "Native network-change notifications unavailable: "
                      << networkMonitorError << '\n';
        }
        // Safety net only: the normal path is event-driven. This also lets a
        // host recover if an operating-system notification was missed.
        drogon::app().getLoop()->runEvery(300.0, refreshNetwork);

        // Reminder delivery is deliberately pull-based: SQLite owns the
        // schedule and this single timer claims a bounded batch. No task
        // objects are retained between ticks, so memory use stays constant.
        drogon::app().getLoop()->runEvery(15.0, [] {
            try {
                const auto dueTasks = ssheila::core::Database::active().claim_due_tasks();
                for (const auto& task : dueTasks) {
                    Json::Value event;
                    event["type"] = "reminder.due";
                    event["taskId"] = task.id;
                    event["title"] = task.title;
                    if (task.reminderAt) {
                        event["reminderAt"] = *task.reminderAt;
                    }
                    ssheila::http::EventsWebSocket::publish(event.toStyledString());
                }
            } catch (const std::exception& error) {
                std::cerr << "Reminder scheduler tick failed: " << error.what() << '\n';
            }
        });

        drogon::app().run();
        networkChangeMonitor.stop();
        mdnsResponder.stop();
    } catch (const std::exception& error) {
        std::cerr << "sSheila failed to start: " << error.what() << '\n';
        return 1;
    }
}
