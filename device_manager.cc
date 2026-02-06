#include "device_manager.hh"

#include <rte_eal.h>
#include <rte_ethdev.h>

#include <stdexcept>

dpdk::device_manager_t::device_manager_t(unsigned int nof_elements, unsigned int cache_size)
    : devices{} {
    // The optimum number of elements in a mempool is n = (2^q - 1)
    if (nof_elements == 0 || (nof_elements & (nof_elements + 1)) != 0) {
        throw std::invalid_argument(
            "Number of elements in mempool must be of the form n = (2^q - 1).");
    }

    // cache_size must be lower or equal to RTE_MEMPOOL_CACHE_MAX_SIZE and n / 1.5
    // It is advised to choose cache_size to have "n modulo cache_size == 0"
    if (cache_size > 0 && (cache_size > RTE_MEMPOOL_CACHE_MAX_SIZE ||
                           cache_size > (nof_elements / 1.5) || (nof_elements % cache_size != 0))) {
        throw std::invalid_argument("Invalid cache size for mempool.");
    }

    // Initialize pool of memory buffers
    mbuf_pool = rte_pktmbuf_pool_create(
        "mbuf_pool", nof_elements, cache_size, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL) {
        throw std::runtime_error("Failed to create mbuf pool.");
    }
}

dpdk::device_manager_t::~device_manager_t() { rte_mempool_free(mbuf_pool); }

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

        devices[port_id] =
            std::unique_ptr<device_t>(new device_t(port_id, pcie_address, mbuf_pool));
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
