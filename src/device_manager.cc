#include "dpdkpp/device_manager.hh"

#include <rte_eal.h>
#include <rte_ethdev.h>

dpdkpp::device_manager_t::~device_manager_t() { rte_mempool_free(mbuf_pool); }

// Static method to initialize EAL
bool dpdkpp::device_manager_t::eal_initialize(int argc, char** argv) {
    return rte_eal_init(argc, argv) >= 0;
}

// Static method to cleanup EAL
bool dpdkpp::device_manager_t::eal_cleanup() { return rte_eal_cleanup() == 0; }

// Refresh the list of managed DPDK devices
bool dpdkpp::device_manager_t::update_device_list() {
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
dpdkpp::device_t* dpdkpp::device_manager_t::get_device_by_port_id(const uint16_t port_id) const {
    if (port_id >= RTE_MAX_ETHPORTS) {
        return nullptr;
    }

    return devices[port_id].get();
}

// Retrieve device by PCIe address
dpdkpp::device_t* dpdkpp::device_manager_t::get_device_by_pcie_address(
    const char* pcie_address) const {
    uint16_t port_id;

    if (rte_eth_dev_get_port_by_name(pcie_address, &port_id) < 0) {
        return nullptr;
    }

    if (port_id >= RTE_MAX_ETHPORTS) {
        return nullptr;
    }

    return devices[port_id].get();
}

struct rte_mbuf* dpdkpp::device_manager_t::alloc_mbuf() { return rte_pktmbuf_alloc(mbuf_pool); }

bool dpdkpp::device_manager_t::alloc_mbufs(struct rte_mbuf** mbufs, unsigned int count) {
    return rte_pktmbuf_alloc_bulk(mbuf_pool, mbufs, count) == 0;
}

struct rte_mbuf* dpdkpp::device_manager_t::clone_mbuf(struct rte_mbuf* mbuf) {
    return rte_pktmbuf_clone(mbuf, mbuf_pool);
}

struct rte_mbuf* dpdkpp::device_manager_t::copy_mbuf(const struct rte_mbuf* mbuf,
                                                     uint32_t offset,
                                                     uint32_t length) {
    return rte_pktmbuf_copy(mbuf, mbuf_pool, offset, length);
}

unsigned int dpdkpp::device_manager_t::get_mbuf_pool_avail_count() const {
    return rte_mempool_avail_count(mbuf_pool);
}

unsigned int dpdkpp::device_manager_t::get_mbuf_pool_in_use_count() const {
    return rte_mempool_in_use_count(mbuf_pool);
}
