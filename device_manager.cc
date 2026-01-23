#include "device_manager.hh"

#include <rte_eal.h>
#include <rte_ethdev.h>

// Static method to initialize EAL
bool dpdk::device_manager_t::eal_initialize(int argc, char** argv) {
    return rte_eal_init(argc, argv) >= 0;
}

// Static method to cleanup EAL
bool dpdk::device_manager_t::eal_cleanup() { return rte_eal_cleanup() == 0; }

// Refresh the list of managed DPDK devices
bool dpdk::device_manager_t::update_device_list() {
    uint16_t port_id;
    char pcie_address[RTE_ETH_NAME_MAX_LEN];

    RTE_ETH_FOREACH_DEV(port_id) {
        if (port_id >= RTE_MAX_ETHPORTS) {
            return false;
        }

        if (devices[port_id].get() != nullptr) {
            continue;
        }

        if (rte_eth_dev_get_name_by_port(port_id, pcie_address) < 0) {
            return false;
        }

        devices[port_id] = std::unique_ptr<device_t>(new device_t(port_id, pcie_address));
    }

    return true;
}

// Retrieve device by port ID
dpdk::device_t* dpdk::device_manager_t::get_device_by_port_id(const uint16_t port_id) const {
    if (port_id >= RTE_MAX_ETHPORTS) {
        return nullptr;
    }

    return devices[port_id].get();
}

// Retrieve device by PCIe address
dpdk::device_t* dpdk::device_manager_t::get_device_by_pcie_address(const char* pcie_address) const {
    uint16_t port_id;

    if (rte_eth_dev_get_port_by_name(pcie_address, &port_id) < 0) {
        return nullptr;
    }

    if (port_id >= RTE_MAX_ETHPORTS) {
        return nullptr;
    }

    return devices[port_id].get();
}
