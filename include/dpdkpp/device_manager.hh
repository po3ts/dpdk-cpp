#pragma once

#include <array>
#include <memory>

#include "dpdkpp/device.hh"

namespace dpdkpp {

class device_manager_t final {
    public:
        ~device_manager_t();

        device_manager_t(device_manager_t&&) = delete;
        device_manager_t(const device_manager_t&) = delete;
        device_manager_t& operator=(const device_manager_t&) = delete;

        template <unsigned int NbElements, unsigned int CacheSize>
        [[nodiscard]] static std::unique_ptr<device_manager_t> create(const char* pool_name) {
            static_assert(NbElements > 0,
                          "Number of elements in mempool must be greater than zero.");
            static_assert((NbElements & (NbElements + 1)) == 0,
                          "Number of elements in mempool must be of the form n = (2^q - 1).");
            static_assert(CacheSize == 0 || CacheSize <= RTE_MEMPOOL_CACHE_MAX_SIZE,
                          "Cache size must be lower or equal to RTE_MEMPOOL_CACHE_MAX_SIZE.");
            static_assert(CacheSize == 0 || CacheSize <= NbElements / 1.5,
                          "Cache size must be lower or equal to n / 1.5.");
            static_assert(CacheSize == 0 || NbElements % CacheSize == 0,
                          "Number of elements modulo cache size must be zero.");

            // Initialize pool of memory buffers
            struct rte_mempool* mbuf_pool = rte_pktmbuf_pool_create(
                pool_name, NbElements, CacheSize, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

            if (mbuf_pool == nullptr) {
                return nullptr;
            }

            return std::unique_ptr<device_manager_t>(new device_manager_t(mbuf_pool));
        }

        // EAL initialization and cleanup
        static bool eal_initialize(int argc, char** argv);
        static bool eal_cleanup();

        // Refresh the list of managed devices
        bool update_device_list();

        // Retrieve device by port ID or PCIe address
        [[nodiscard]] device_t* get_device_by_port_id(const uint16_t port_id) const;
        [[nodiscard]] device_t* get_device_by_pcie_address(const char* pcie_address) const;

        // Allocate one or more mbufs from the pool
        [[nodiscard]] struct rte_mbuf* alloc_mbuf();
        [[nodiscard]] bool alloc_mbufs(struct rte_mbuf** mbufs, unsigned int count);

        // Clone or copy an mbuf using the pool
        [[nodiscard]] struct rte_mbuf* clone_mbuf(struct rte_mbuf* mbuf);
        [[nodiscard]] struct rte_mbuf* copy_mbuf(const struct rte_mbuf* mbuf,
                                                 uint32_t offset,
                                                 uint32_t length);

        // Query pool utilization
        unsigned int get_mbuf_pool_avail_count() const;
        unsigned int get_mbuf_pool_in_use_count() const;

    private:
        device_manager_t(struct rte_mempool* mbuf_pool)
            : mbuf_pool{mbuf_pool},
              devices{} {}

        struct rte_mempool* mbuf_pool;
        std::array<std::unique_ptr<dpdkpp::device_t>, RTE_MAX_ETHPORTS> devices;
};

}  // namespace dpdkpp
