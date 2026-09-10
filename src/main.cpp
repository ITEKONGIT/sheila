#include "ssheila/core/database.hpp"
#include "ssheila/core/local_subnet_access.hpp"
#include "ssheila/core/storage_layout.hpp"

#include <drogon/drogon.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct Options {
    std::string address{"0.0.0.0"};
    std::uint16_t port{18877};
    std::filesystem::path dataRoot{ssheila::core::default_data_root()};
};

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

        const auto subnetAccess = std::make_shared<const ssheila::core::LocalSubnetAccess>(
            ssheila::core::LocalSubnetAccess::discover());

        std::cout << "sSheila 0.1.0\n"
                  << "Storage: " << layout.root << '\n'
                  << "Local: http://127.0.0.1:" << options.port << '\n';
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
            .setClientMaxBodySize(32ULL * 1024ULL * 1024ULL)
            .setThreadNum(0);

        drogon::app().registerPreRoutingAdvice(
            [subnetAccess](const drogon::HttpRequestPtr& request,
                           drogon::AdviceCallback&& reject,
                           drogon::AdviceChainCallback&& continueRequest) {
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

        drogon::app().run();
    } catch (const std::exception& error) {
        std::cerr << "sSheila failed to start: " << error.what() << '\n';
        return 1;
    }
}
