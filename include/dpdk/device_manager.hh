#pragma once

#include <array>
#include <memory>

#include "dpdk/device.hh"

namespace dpdk {

class device_manager_t final {
    public:
        device_manager_t(unsigned int nb_elements, unsigned int cache_size);
        ~device_manager_t();

        device_manager_t(const device_manager_t&) = delete;
        device_manager_t& operator=(const device_manager_t&) = delete;

        void cleanup();

        // EAL initialization and cleanup
        static bool eal_initialize(int argc, char** argv);
        static bool eal_cleanup();

        // Refresh the list of managed devices
        bool update_device_list();

        // Retrieve device by port ID or PCIe address
        [[nodiscard]] device_t* get_device_by_port_id(const uint16_t port_id) const;
        [[nodiscard]] device_t* get_device_by_pcie_address(const char* pcie_address) const;

    private:
        struct rte_mempool* mbuf_pool;
        std::array<std::unique_ptr<dpdk::device_t>, RTE_MAX_ETHPORTS> devices;
};

}  // namespace dpdk
