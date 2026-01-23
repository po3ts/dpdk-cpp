#pragma once

#include <rte_ether.h>

#include <ostream>

inline std::ostream& operator<<(std::ostream& os, const struct rte_ether_addr& addr) {
    char buf[RTE_ETHER_ADDR_FMT_SIZE];
    rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, &addr);
    os << buf;
    return os;
}
