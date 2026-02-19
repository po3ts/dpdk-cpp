#pragma once

#include <rte_ethdev.h>
#include <rte_ether.h>

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>

namespace dpdkpp {

enum class device_status_t : uint8_t {
    UNCONFIGURED,
    CONFIGURED,
    READY,
    RUNNING
};

class device_t {
    public:
        device_t(device_t&&) = delete;
        device_t(const device_t&) = delete;
        device_t& operator=(const device_t&) = delete;

        uint16_t get_port_id() const { return port_id; }
        const char* get_pcie_address() const { return pcie_address.data(); }
        std::optional<struct rte_ether_addr> get_mac_address() const;

        device_status_t get_device_status() const { return device_status; }

        // Get the number of configured Rx/Tx queues
        uint16_t get_nb_rx_queues() const { return nb_rx_queues; }
        uint16_t get_nb_tx_queues() const { return nb_tx_queues; }

        // Get per-queue packet statistics
        uint64_t get_nb_packets_received(uint16_t queue_id) const;
        uint64_t get_nb_packets_transmitted(uint16_t queue_id) const;
        uint64_t get_nb_packets_dropped(uint16_t queue_id) const;

        bool configure(const uint16_t nb_rx_queues,
                       const uint16_t nb_tx_queues,
                       const rte_eth_conf* eth_conf);

        bool setup_rx_tx_queues(uint16_t nb_rx_descriptors,
                                uint16_t nb_tx_descriptors,
                                const rte_eth_rxconf* rx_conf,
                                const rte_eth_txconf* tx_conf);

        bool setup_tx_buffers(const size_t nb_packets_per_tx_buffer);

        // Free Tx buffers allocated by setup_tx_buffers
        void free_tx_buffers();

        // Receive a burst of packets from an Rx queue
        uint16_t rx_burst(uint16_t queue_id, struct rte_mbuf** pkts, uint16_t nb_pkts);

        // Transmit a burst of packets on a Tx queue
        uint16_t tx_burst(uint16_t queue_id, struct rte_mbuf** pkts, uint16_t nb_pkts);

        // Buffer a single packet for transmission on a Tx queue
        uint16_t tx_buffer(uint16_t queue_id, struct rte_mbuf* pkt);

        // Flush the Tx buffer for a given Tx queue
        uint16_t tx_buffer_flush(uint16_t queue_id);

        bool start();
        bool stop();

        void reset();

        template <auto Func,
                  typename... Args,
                  std::enable_if_t<std::is_invocable_r_v<int, decltype(Func), uint16_t, Args...>,
                                   int> = 0>
        int invoke(Args&&... args) {
            static_assert(!is_blacklisted<rte_eth_dev_close, Func>(),
                          "The destructor takes care of closing the device");
            static_assert(!is_blacklisted<rte_eth_dev_configure, Func>(),
                          "Use configure() instead of invoking rte_eth_dev_configure directly");
            static_assert(!is_blacklisted<rte_eth_dev_reset, Func>(),
                          "Use reset() instead of invoking rte_eth_dev_reset directly");
            static_assert(!is_blacklisted<rte_eth_dev_start, Func>(),
                          "Use start() instead of invoking rte_eth_dev_start directly");
            static_assert(!is_blacklisted<rte_eth_dev_stop, Func>(),
                          "Use stop() instead of invoking rte_eth_dev_stop directly");
            static_assert(
                !is_blacklisted<rte_eth_tx_queue_setup, Func>(),
                "Use setup_rx_tx_queues() instead of invoking rte_eth_tx_queue_setup directly");
            return Func(port_id, std::forward<Args>(args)...);
        }

        friend class device_manager_t;
        friend struct std::default_delete<device_t>;

    private:
        device_t(const uint16_t port_id, const char* pcie_address, struct rte_mempool* mbuf_pool);
        ~device_t();

        // Custom error callback for Tx buffers (replaces rte_eth_tx_buffer_count_callback)
        static void tx_buffer_count_callback(struct rte_mbuf** pkts,
                                             uint16_t unsent,
                                             void* userdata);

        template <auto Blacklisted, auto Func>
        static constexpr bool is_blacklisted() {
            if constexpr (std::is_same_v<decltype(Func), decltype(Blacklisted)>) {
                return Func == Blacklisted;
            } else {
                return false;
            }
        }

        const uint16_t port_id;
        std::array<char, RTE_ETH_NAME_MAX_LEN> pcie_address;
        struct rte_mempool* mbuf_pool;

        device_status_t device_status;

        uint16_t nb_rx_queues;
        uint16_t nb_tx_queues;

        // Statistics for each queue, indexed by queue ID
        std::unique_ptr<std::atomic<uint64_t>[]> nb_packets_received;
        std::unique_ptr<std::atomic<uint64_t>[]> nb_packets_transmitted;
        std::unique_ptr<std::atomic<uint64_t>[]> nb_packets_dropped;

        std::unique_ptr<struct rte_eth_dev_tx_buffer*[]> tx_buffers;
};

}  // namespace dpdkpp
