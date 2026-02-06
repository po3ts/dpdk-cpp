#pragma once

#include <rte_ethdev.h>
#include <rte_ether.h>

#include <memory>
#include <optional>
#include <type_traits>

namespace dpdk {

class device_t {
    public:
        device_t(const device_t&) = delete;
        device_t& operator=(const device_t&) = delete;

        const uint16_t get_port_id() const { return port_id; }
        const char* get_pcie_address() const { return pcie_address; }
        std::optional<struct rte_ether_addr> get_mac_address() const;

        bool configure(const uint16_t nb_rx_queues,
                       const uint16_t nb_tx_queues,
                       const rte_eth_conf* eth_conf);

        bool setup_rx_tx_queues(uint16_t nb_rx_descriptors,
                                uint16_t nb_tx_descriptors,
                                const rte_eth_rxconf* rx_conf,
                                const rte_eth_txconf* tx_conf);

        bool setup_tx_buffers(const size_t nb_packets_per_buffer);

        template <typename Func,
                  typename... Args,
                  std::enable_if_t<std::is_invocable_r_v<int, Func, uint16_t, Args...>, int> = 0>
        int invoke(Func func, Args&&... args) {
            return func(port_id, std::forward<Args>(args)...);
        }

        friend class device_manager_t;
        friend struct std::default_delete<device_t>;

    private:
        device_t(const uint16_t port_id, const char* pcie_address, struct rte_mempool* mbuf_pool);
        ~device_t();

        const uint16_t port_id;
        char pcie_address[RTE_ETH_NAME_MAX_LEN];

        struct rte_mempool* mbuf_pool;

        uint16_t nb_rx_queues;
        uint16_t nb_tx_queues;

        std::unique_ptr<struct rte_eth_dev_tx_buffer*[]> tx_buffers;
};

}  // namespace dpdk
