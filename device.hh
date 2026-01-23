#pragma once

#include <rte_ethdev.h>
#include <rte_ether.h>

#include <memory>
#include <optional>

namespace dpdk {

class device_t {
    public:
        device_t(const device_t&) = delete;
        device_t& operator=(const device_t&) = delete;

        const uint16_t get_port_id() const { return port_id; }
        const char* get_pcie_address() const { return pcie_address; }
        std::optional<struct rte_ether_addr> get_mac_address() const;

        friend class device_manager_t;
        friend struct std::default_delete<device_t>;

    private:
        device_t(const uint16_t port_id, const char* pcie_address);
        ~device_t() = default;

        const uint16_t port_id;
        char pcie_address[RTE_ETH_NAME_MAX_LEN];
};

}  // namespace dpdk
