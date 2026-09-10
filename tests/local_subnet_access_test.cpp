#include "ssheila/core/local_subnet_access.hpp"

#include <cassert>
#include <iostream>

int main() {
    const auto access = ssheila::core::LocalSubnetAccess::from_cidrs({
        "192.168.20.14/24",
        "10.42.0.8/16",
    });

    assert(access.allows("127.0.0.1"));
    assert(access.allows("192.168.20.1"));
    assert(access.allows("192.168.20.254"));
    assert(access.allows("10.42.99.4"));
    assert(!access.allows("192.168.21.1"));
    assert(!access.allows("10.43.0.1"));
    assert(!access.allows("8.8.8.8"));
    assert(!access.allows("not-an-address"));

    const auto cidrs = access.cidrs();
    assert(cidrs.size() == 2);
    assert(cidrs[0] == "10.42.0.0/16");
    assert(cidrs[1] == "192.168.20.0/24");

    std::cout << "local subnet access test passed\n";
}
