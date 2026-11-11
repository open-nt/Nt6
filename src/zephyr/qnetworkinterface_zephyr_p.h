// MIT License - see LICENSE
#pragma once

// What QNetworkInterface needs to know about the interfaces of the Zephyr network stack, in
// plain types: the file that reads them includes the Zephyr headers, whose macros are best kept
// away from the Qt ones.

#include <string>
#include <vector>

namespace QtPrivate {

struct ZephyrAddress {
    bool ipv6 = false;
    unsigned char ip[16] = {};   // four bytes of an IPv4 address, in network order
    unsigned char mask[16] = {}; // of an IPv4 address only
    bool hasMask = false;
};

struct ZephyrInterface {
    std::string name;
    int index = 0;
    int mtu = 0;
    bool up = false;
    bool running = false;
    bool ethernet = false;
    bool wifi = false;
    std::vector<unsigned char> hardwareAddress;
    std::vector<ZephyrAddress> addresses;
};

std::vector<ZephyrInterface> zephyrInterfaces();

} // namespace QtPrivate
