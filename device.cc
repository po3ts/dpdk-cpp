#include "device.hh"

std::optional<struct rte_ether_addr> dpdk::device_t::get_mac_address() const {
    struct rte_ether_addr mac_addr;

    if (rte_eth_macaddr_get(port_id, &mac_addr) < 0) {
        return std::nullopt;
    }

    return mac_addr;
}

dpdk::device_t::device_t(const uint16_t port_id, const char* pcie_address)
    : port_id(port_id) {
    strlcpy(this->pcie_address, pcie_address, RTE_ETH_NAME_MAX_LEN);
}
