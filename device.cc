#include "device.hh"

#include <rte_malloc.h>

std::optional<struct rte_ether_addr> dpdk::device_t::get_mac_address() const {
    struct rte_ether_addr mac_addr;

    if (rte_eth_macaddr_get(port_id, &mac_addr) < 0) {
        return std::nullopt;
    }

    return mac_addr;
}

bool dpdk::device_t::configure(const uint16_t nb_rx_queues,
                               const uint16_t nb_tx_queues,
                               const rte_eth_conf* eth_conf) {
    this->nb_rx_queues = nb_rx_queues;
    this->nb_tx_queues = nb_tx_queues;

    return rte_eth_dev_configure(port_id, nb_rx_queues, nb_tx_queues, eth_conf) == 0;
}

bool dpdk::device_t::setup_rx_tx_queues(uint16_t nb_rx_descriptors,
                                        uint16_t nb_tx_descriptors,
                                        const rte_eth_rxconf* rx_conf,
                                        const rte_eth_txconf* tx_conf) {
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

    return true;
}

bool dpdk::device_t::setup_tx_buffers(const size_t nb_packets_per_buffer) {
    if (tx_buffers != nullptr) {
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
            RTE_ETH_TX_BUFFER_SIZE(nb_packets_per_buffer),
            0,
            rte_eth_dev_socket_id(port_id));

        if (tx_buffers[tx_queue_id] == NULL) {
            return false;
        }

        rte_eth_tx_buffer_init(tx_buffers[tx_queue_id], nb_packets_per_buffer);
    }

    return true;
}

dpdk::device_t::device_t(const uint16_t port_id,
                         const char* pcie_address,
                         struct rte_mempool* mbuf_pool)
    : port_id(port_id),
      mbuf_pool(mbuf_pool) {
    strlcpy(this->pcie_address, pcie_address, RTE_ETH_NAME_MAX_LEN);
}

dpdk::device_t::~device_t() {
    // Free Tx buffers
    if (tx_buffers != nullptr) {
        for (uint16_t tx_queue_id = 0; tx_queue_id < nb_tx_queues; ++tx_queue_id) {
            if (tx_buffers[tx_queue_id] != nullptr) {
                rte_free(tx_buffers[tx_queue_id]);
            }
        }
        // unique_ptr automatically calls delete[] on the array
    }
}
