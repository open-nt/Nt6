// MIT License - see LICENSE
// Lists the interfaces of the Zephyr network stack for QNetworkInterface.
#include "qnetworkinterface_zephyr_p.h"

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_l2.h>

#include <cstdio>
#include <cstring>

namespace QtPrivate {

namespace {

void addInterface(struct net_if *iface, void *userData) {
    auto *list = static_cast<std::vector<ZephyrInterface> *>(userData);
    ZephyrInterface out;
    out.index = net_if_get_by_iface(iface);
    out.mtu = net_if_get_mtu(iface);
    out.up = net_if_is_up(iface);
    out.running = out.up && net_if_is_carrier_ok(iface);

    char name[32] = {};
#if defined(CONFIG_NET_INTERFACE_NAME)
    if (net_if_get_name(iface, name, static_cast<int>(sizeof name) - 1) <= 0)
        name[0] = '\0';
#endif
    if (name[0] == '\0')
        std::snprintf(name, sizeof name, "net%d", out.index);
    out.name = name;

#if defined(CONFIG_NET_L2_ETHERNET)
    out.ethernet = net_if_l2(iface) == &NET_L2_GET_NAME(ETHERNET);
#endif
#if defined(CONFIG_WIFI)
    out.wifi = net_if_is_wifi(iface);
#endif

    if (const struct net_linkaddr *link = net_if_get_link_addr(iface))
        out.hardwareAddress.assign(link->addr, link->addr + link->len);

#if defined(CONFIG_NET_IPV4)
    if (const struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4) {
        for (const struct net_if_addr_ipv4 &unicast : ipv4->unicast) {
            if (!unicast.ipv4.is_used)
                continue;
            ZephyrAddress address;
            std::memcpy(address.ip, &unicast.ipv4.address.in_addr, 4);
            std::memcpy(address.mask, &unicast.netmask, 4);
            address.hasMask = true;
            out.addresses.push_back(address);
        }
    }
#endif
#if defined(CONFIG_NET_IPV6)
    if (const struct net_if_ipv6 *ipv6 = iface->config.ip.ipv6) {
        for (const struct net_if_addr &unicast : ipv6->unicast) {
            if (!unicast.is_used)
                continue;
            ZephyrAddress address;
            address.ipv6 = true;
            std::memcpy(address.ip, &unicast.address.in6_addr, 16);
            out.addresses.push_back(address);
        }
    }
#endif
    list->push_back(std::move(out));
}

} // namespace

std::vector<ZephyrInterface> zephyrInterfaces() {
    std::vector<ZephyrInterface> list;
    net_if_foreach(&addInterface, &list);
    return list;
}

} // namespace QtPrivate
