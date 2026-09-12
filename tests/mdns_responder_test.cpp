#include "ssheila/core/mdns_responder.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main() {
    assert(std::string{ssheila::core::kMdnsHostname} == "sheila.local");
    assert(ssheila::core::kMdnsPort == 5353);

    ssheila::core::MdnsResponder responder{std::vector<std::string>{}};
    std::string error;
    assert(!responder.start(error));
    assert(error.find("No active IPv4 interface") != std::string::npos);

    std::cout << "mDNS responder validation test passed\n";
}
