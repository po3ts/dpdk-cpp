#include "dpdkpp/device.hh"

#include <rte_malloc.h>

std::optional<struct rte_ether_addr> dpdkpp::device_t::get_mac_address() const {
    struct rte_ether_addr mac_addr;

    if (rte_eth_macaddr_get(port_id, &mac_addr) < 0) {
        return std::nullopt;
    }

    return mac_addr;
}

uint64_t dpdkpp::device_t::get_nb_packets_received(uint16_t queue_id) const {
    if (queue_id >= nb_rx_queues) {
        return 0;
    }

    return nb_packets_received[queue_id].load(std::memory_order_relaxed);
}

uint64_t dpdkpp::device_t::get_nb_packets_transmitted(uint16_t queue_id) const {
    if (queue_id >= nb_tx_queues) {
        return 0;
    }

    return nb_packets_transmitted[queue_id].load(std::memory_order_relaxed);
}

uint64_t dpdkpp::device_t::get_nb_packets_dropped(uint16_t queue_id) const {
    if (queue_id >= nb_tx_queues) {
        return 0;
    }

    return nb_packets_dropped[queue_id].load(std::memory_order_relaxed);
}

bool dpdkpp::device_t::configure(const uint16_t nb_rx_queues,
                                 const uint16_t nb_tx_queues,
                                 const rte_eth_conf* eth_conf) {
    // Verify that device is not yet configured
    if (device_status != device_status_t::UNCONFIGURED) {
        return false;
    }

    // Verify number of Rx queues is in range [1, RTE_MAX_QUEUES_PER_PORT]
    if (nb_rx_queues == 0 || nb_rx_queues > RTE_MAX_QUEUES_PER_PORT) {
        return false;
    }

    // Verify number of Tx queues is in range [1, RTE_MAX_QUEUES_PER_PORT]
    if (nb_tx_queues == 0 || nb_tx_queues > RTE_MAX_QUEUES_PER_PORT) {
        return false;
    }

    // Verify that eth_conf is not null
    if (eth_conf == nullptr) {
        return false;
    }

    // Call DPDK to configure the device
    if (rte_eth_dev_configure(port_id, nb_rx_queues, nb_tx_queues, eth_conf) < 0) {
        return false;
    }

    // Initialize the statistics arrays
    nb_packets_received = std::make_unique<std::atomic<uint64_t>[]>(nb_rx_queues);
    nb_packets_transmitted = std::make_unique<std::atomic<uint64_t>[]>(nb_tx_queues);
    nb_packets_dropped = std::make_unique<std::atomic<uint64_t>[]>(nb_tx_queues);

    this->nb_rx_queues = nb_rx_queues;
    this->nb_tx_queues = nb_tx_queues;

    device_status = device_status_t::CONFIGURED;
    return true;
}

bool dpdkpp::device_t::setup_rx_tx_queues(uint16_t nb_rx_descriptors,
                                          uint16_t nb_tx_descriptors,
                                          const rte_eth_rxconf* rx_conf,
                                          const rte_eth_txconf* tx_conf) {
    // Verify that device is configured
    if (device_status != device_status_t::CONFIGURED) {
        return false;
    }

    // Adjust the number of descriptors if necessary
    if (rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rx_descriptors, &nb_tx_descriptors) < 0) {
        return false;
    }

    // Set up Rx queues
    for (uint16_t rx_queue_id = 0; rx_queue_id < nb_rx_queues; ++rx_queue_id) {
        if (rte_eth_rx_queue_setup(port_id,
                                   rx_queue_id,
                                   nb_rx_descriptors,
                                   rte_eth_dev_socket_id(port_id),
                                   rx_conf,
                                   mbuf_pool) < 0) {
            return false;
        }
    }

    // Set up Tx queues
    for (uint16_t tx_queue_id = 0; tx_queue_id < nb_tx_queues; ++tx_queue_id) {
        if (rte_eth_tx_queue_setup(
                port_id, tx_queue_id, nb_tx_descriptors, rte_eth_dev_socket_id(port_id), tx_conf) <
            0) {
            return false;
        }
    }

    device_status = device_status_t::READY;
    return true;
}

bool dpdkpp::device_t::setup_tx_buffers(const size_t nb_packets_per_tx_buffer) {
    // Verify that device is configured or ready
    if (device_status != device_status_t::CONFIGURED && device_status != device_status_t::READY) {
        return false;
    }

    // Verify that Tx buffers have not yet been set up
    if (tx_buffers != nullptr) {
        return false;
    }

    // Number of packets per Tx buffer must not be zero
    if (nb_packets_per_tx_buffer == 0) {
        return false;
    }

    // One Tx buffer per Tx queue
    tx_buffers = std::make_unique<struct rte_eth_dev_tx_buffer*[]>(nb_tx_queues);

    // Allocate and initialize Tx buffers
    for (uint16_t tx_queue_id = 0; tx_queue_id < nb_tx_queues; ++tx_queue_id) {
        // String can have a maximum size of 22 bytes ("tx_buffer_65535_65535\0"),
        // so 32 bytes is comfortably safe
        char tx_buffer_name[32];
        snprintf(tx_buffer_name, sizeof(tx_buffer_name), "tx_buffer_%u_%u", port_id, tx_queue_id);

        tx_buffers[tx_queue_id] = (struct rte_eth_dev_tx_buffer*)rte_zmalloc_socket(
            tx_buffer_name,
            RTE_ETH_TX_BUFFER_SIZE(nb_packets_per_tx_buffer),
            0,
            rte_eth_dev_socket_id(port_id));

        if (tx_buffers[tx_queue_id] == nullptr) {
            // Failed to allocate memory for Tx buffer
            free_tx_buffers();
            return false;
        }

        // Initialize Tx buffer
        rte_eth_tx_buffer_init(tx_buffers[tx_queue_id], nb_packets_per_tx_buffer);

        // Set error callback for Tx buffer
        if (rte_eth_tx_buffer_set_err_callback(tx_buffers[tx_queue_id],
                                               tx_buffer_count_callback,
                                               &nb_packets_dropped[tx_queue_id]) < 0) {
            free_tx_buffers();
            return false;
        }
    }

    return true;
}

void dpdkpp::device_t::free_tx_buffers() {
    if (tx_buffers == nullptr) {
        return;
    }

    for (uint16_t tx_queue_id = 0; tx_queue_id < nb_tx_queues; ++tx_queue_id) {
        if (tx_buffers[tx_queue_id] != nullptr) {
            rte_free(tx_buffers[tx_queue_id]);
        }
    }

    // unique_ptr automatically calls delete[] on the array
    tx_buffers.reset();
}

uint16_t dpdkpp::device_t::rx_burst(uint16_t queue_id, struct rte_mbuf** pkts, uint16_t nb_pkts) {
    // Device must be running to receive burst of packets
    if (device_status != device_status_t::RUNNING) {
        return 0;
    }

    // Queue ID must be in the range [0, nb_rx_queues - 1] previously supplied to configure()
    if (queue_id >= nb_rx_queues) {
        return 0;
    }

    uint16_t nb_rx = rte_eth_rx_burst(port_id, queue_id, pkts, nb_pkts);
    nb_packets_received[queue_id].fetch_add(nb_rx, std::memory_order_relaxed);
    return nb_rx;
}

uint16_t dpdkpp::device_t::tx_burst(uint16_t queue_id, struct rte_mbuf** pkts, uint16_t nb_pkts) {
    // Device must be running to transmit burst of packets
    if (device_status != device_status_t::RUNNING) {
        return 0;
    }

    // Queue ID must be in the range [0, nb_tx_queues - 1] previously supplied to configure()
    if (queue_id >= nb_tx_queues) {
        return 0;
    }

    uint16_t nb_tx = rte_eth_tx_burst(port_id, queue_id, pkts, nb_pkts);
    nb_packets_transmitted[queue_id].fetch_add(nb_tx, std::memory_order_relaxed);
    return nb_tx;
}

uint16_t dpdkpp::device_t::tx_buffer(uint16_t queue_id, struct rte_mbuf* pkt) {
    // Device must be running to buffer a Tx packet
    if (device_status != device_status_t::RUNNING) {
        return 0;
    }

    // Tx buffers must have been set up to buffer a packet for transmission
    if (tx_buffers == nullptr) {
        return 0;
    }

    // Queue ID must be in the range [0, nb_tx_queues - 1] previously supplied to configure()
    if (queue_id >= nb_tx_queues) {
        return 0;
    }

    uint16_t nb_tx = rte_eth_tx_buffer(port_id, queue_id, tx_buffers[queue_id], pkt);
    nb_packets_transmitted[queue_id].fetch_add(nb_tx, std::memory_order_relaxed);
    return nb_tx;
}

uint16_t dpdkpp::device_t::tx_buffer_flush(uint16_t queue_id) {
    // Device must be running to flush Tx buffer
    if (device_status != device_status_t::RUNNING) {
        return 0;
    }

    // Tx buffers must have been set up before flushing
    if (tx_buffers == nullptr) {
        return 0;
    }

    // Queue ID must be in the range [0, nb_tx_queues - 1] previously supplied to configure()
    if (queue_id >= nb_tx_queues) {
        return 0;
    }

    uint16_t nb_tx = rte_eth_tx_buffer_flush(port_id, queue_id, tx_buffers[queue_id]);
    nb_packets_transmitted[queue_id].fetch_add(nb_tx, std::memory_order_relaxed);
    return nb_tx;
}

bool dpdkpp::device_t::start() {
    if (device_status != device_status_t::READY) {
        return false;
    }

    if (rte_eth_dev_start(port_id) < 0) {
        return false;
    }

    device_status = device_status_t::RUNNING;
    return true;
}

bool dpdkpp::device_t::stop() {
    if (device_status != device_status_t::RUNNING) {
        return false;
    }

    if (rte_eth_dev_stop(port_id) < 0) {
        return false;
    }

    device_status = device_status_t::READY;
    return true;
}

void dpdkpp::device_t::reset() {
    // If device is unconfigured, there is nothing to reset
    if (device_status == device_status_t::UNCONFIGURED) {
        return;
    }

    rte_eth_dev_reset(port_id);
    free_tx_buffers();

    nb_packets_received.reset();
    nb_packets_transmitted.reset();
    nb_packets_dropped.reset();

    nb_rx_queues = 0;
    nb_tx_queues = 0;

    device_status = device_status_t::UNCONFIGURED;
}

dpdkpp::device_t::device_t(const uint16_t port_id,
                           const char* pcie_address,
                           struct rte_mempool* mbuf_pool)
    : port_id(port_id),
      pcie_address{""},
      mbuf_pool(mbuf_pool),
      device_status(device_status_t::UNCONFIGURED),
      nb_rx_queues(0),
      nb_tx_queues(0),
      nb_packets_received(nullptr),
      nb_packets_transmitted(nullptr),
      nb_packets_dropped(nullptr),
      tx_buffers(nullptr) {
    strlcpy(this->pcie_address.data(), pcie_address, RTE_ETH_NAME_MAX_LEN);
}

dpdkpp::device_t::~device_t() {
    stop();
    rte_eth_dev_close(port_id);
    free_tx_buffers();
}

void dpdkpp::device_t::tx_buffer_count_callback(struct rte_mbuf** pkts,
                                                uint16_t unsent,
                                                void* userdata) {
    auto* counter = static_cast<std::atomic<uint64_t>*>(userdata);
    counter->fetch_add(unsent, std::memory_order_relaxed);

    for (uint16_t i = 0; i < unsent; ++i) {
        rte_pktmbuf_free(pkts[i]);
    }
}
