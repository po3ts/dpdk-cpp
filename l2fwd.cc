#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_pci.h>
#include <signal.h>

#include <iostream>

#include "ptp_hdr.hh"

static volatile bool force_quit;

static void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\n\nSignal %d received, preparing to exit...\n", signum);
        force_quit = true;
    }
}

static void parse_ptpv1_header(struct rte_mbuf* mbuf) {
    struct rte_ipv4_hdr* ipv4_hdr;
    struct ptpv1_header* ptp_hdr;

    if (mbuf->packet_type & RTE_PTYPE_L3_IPV4) {
        ipv4_hdr =
            rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr*, sizeof(struct rte_ether_hdr));

        // Check DSCP for PTP
        // https://www.getdante.com/support/faq/how-does-dante-use-dscp-diffserv-priority-values-when-configuring-qos/
        if (ipv4_hdr->type_of_service >> 2 == 0x38) {
            size_t ptp_hdr_offset =
                sizeof(struct rte_ether_hdr) + ipv4_hdr->ihl * 4 + sizeof(rte_udp_hdr);
            ptp_hdr = rte_pktmbuf_mtod_offset(mbuf, struct ptpv1_header*, ptp_hdr_offset);
            RTE_LOG(INFO,
                    PORT,
                    PTP_SOURCE_UUID_PRT_FMT "[%04d]: %s.\n",
                    PTP_SOURCE_UUID_BYTES(ptp_hdr->sourceUuid),
                    rte_be_to_cpu_16(ptp_hdr->sequenceId),
                    PTP_CONTROL_FIELD_STRING(ptp_hdr->control));
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "This is a simple layer 2 forwarding application built around DPDK." << std::endl;

    if (rte_eal_init(argc, argv) < 0) {
        rte_exit(EXIT_FAILURE, "Cannot initialize Environment Abstraction Layer (EAL).\n");
    }

    if (rte_eth_dev_count_avail() == 0) {
        rte_exit(EXIT_FAILURE, "No Ethernet devices available.\n");
    }

    char const* dev_name[] = {"0000:02:00.2", "0000:02:00.3"};
    size_t const dev_count = sizeof(dev_name) / sizeof(dev_name[0]);
    uint16_t port_ids[dev_count];
    struct rte_ether_addr mac_addrs[dev_count];
    struct rte_eth_dev_tx_buffer* tx_bufs[dev_count];
    uint64_t pkt_count_dropped[dev_count];

    for (int i = 0; i < dev_count; ++i) {
        if (rte_eth_dev_get_port_by_name(dev_name[i], &port_ids[i]) < 0) {
            rte_exit(EXIT_FAILURE, "Device %s not found.\n", dev_name[i]);
        }

        if (rte_eth_macaddr_get(port_ids[i], &mac_addrs[i]) < 0) {
            rte_exit(EXIT_FAILURE, "Cannot get MAC address for port %u.\n", port_ids[i]);
        }

        RTE_LOG(INFO,
                PORT,
                "MAC address of port %u is " RTE_ETHER_ADDR_PRT_FMT ".\n",
                port_ids[i],
                RTE_ETHER_ADDR_BYTES(&mac_addrs[i]));

        tx_bufs[i] = (struct rte_eth_dev_tx_buffer*)rte_zmalloc_socket(
            NULL, RTE_ETH_TX_BUFFER_SIZE(32), 0, rte_eth_dev_socket_id(port_ids[i]));
        if (tx_bufs[i] == NULL) {
            rte_exit(
                EXIT_FAILURE, "Cannot allocate memory for Tx buffer of port %u.\n", port_ids[i]);
        }
    }

    // Initialize pool of memory buffers
    // It is advised to choose cache_size to have "n modulo cache_size == 0"
    struct rte_mempool* mempool = rte_pktmbuf_pool_create(
        "pktmbuf_pool", 4095U, 273U, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mempool == NULL) {
        rte_exit(EXIT_FAILURE, "Failed to initialize pool of memory buffers.\n");
    }

    for (auto const port_id : port_ids) {
        struct rte_eth_dev_info dev_info;
        if (rte_eth_dev_info_get(port_id, &dev_info) != 0) {
            rte_exit(EXIT_FAILURE, "Could not get device info for port %u.\n", port_id);
        }

        struct rte_eth_conf port_conf {};
        port_conf.txmode.mq_mode = RTE_ETH_MQ_TX_NONE;
        port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

        if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) {
            port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
        }

        // https://www.intel.com/content/www/us/en/support/articles/000054836/ethernet-products.html
        if (dev_info.rx_offload_capa & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
            port_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;
        }

        if (rte_eth_dev_configure(port_id, 1, 1, &port_conf) < 0) {
            rte_exit(EXIT_FAILURE, "Failed to configure device with port %u.\n", port_id);
        }

        uint16_t nb_rx_desc = 1024;
        uint16_t nb_tx_desc = 1024;

        if (rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rx_desc, &nb_tx_desc) < 0) {
            rte_exit(
                EXIT_FAILURE, "Cannot adjust number of Rx/Tx descriptors for port %u.\n", port_id);
        }

        // Set up single Rx queue
        struct rte_eth_rxconf rx_conf = dev_info.default_rxconf;
        rx_conf.offloads = port_conf.rxmode.offloads;
        if (rte_eth_rx_queue_setup(
                port_id, 0, nb_rx_desc, rte_eth_dev_socket_id(port_id), &rx_conf, mempool) < 0) {
            rte_exit(EXIT_FAILURE, "Failed to set up Rx queue for port %u.\n", port_id);
        }

        // Same procedure for the Tx queue
        struct rte_eth_txconf tx_conf = dev_info.default_txconf;
        tx_conf.offloads = port_conf.txmode.offloads;
        if (rte_eth_tx_queue_setup(
                port_id, 0, nb_tx_desc, rte_eth_dev_socket_id(port_id), &tx_conf) < 0) {
            rte_exit(EXIT_FAILURE, "Failed to set up Tx queue for port %u.\n", port_id);
        }

        // Initialize Tx buffer
        rte_eth_tx_buffer_init(tx_bufs[port_id], 32);

        // Set error callback for Tx buffer
        if (rte_eth_tx_buffer_set_err_callback(tx_bufs[port_id],
                                               rte_eth_tx_buffer_count_callback,
                                               &pkt_count_dropped[port_id]) < 0) {
            rte_exit(
                EXIT_FAILURE, "Failed to set error callback for Tx buffer on port %u.\n", port_id);
        }

        // Start Ethernet device
        if (rte_eth_dev_start(port_id) < 0) {
            rte_exit(EXIT_FAILURE, "Failed to start Ethernet device on port %u.\n", port_id);
        }

        // Enable PTP timestamping
        if (rte_eth_timesync_enable(port_id) < 0) {
            RTE_LOG(ALERT, PORT, "Failed to enable PTP timestamping on port %u.\n", port_id);
        }

        // Enable promiscuous mode
        if (rte_eth_promiscuous_enable(port_id) != 0) {
            rte_exit(EXIT_FAILURE, "Failed to enable promiscuous mode on port %u.\n", port_id);
        }
    }

    force_quit = false;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    uint16_t nb_rx;
    struct rte_mbuf* pkts_burst[32];

    while (likely(!force_quit)) {
        // Port 0 -> port 1
        rte_eth_tx_buffer_flush(port_ids[1], 0, tx_bufs[1]);

        nb_rx = rte_eth_rx_burst(port_ids[0], 0, pkts_burst, 32);

        for (uint16_t i = 0; i < nb_rx; ++i) {
            parse_ptpv1_header(pkts_burst[i]);
            rte_eth_tx_buffer(port_ids[1], 0, tx_bufs[1], pkts_burst[i]);
        }

        // Port 1 -> port 0
        rte_eth_tx_buffer_flush(port_ids[0], 0, tx_bufs[0]);

        nb_rx = rte_eth_rx_burst(port_ids[1], 0, pkts_burst, 32);

        for (uint16_t i = 0; i < nb_rx; ++i) {
            parse_ptpv1_header(pkts_burst[i]);
            rte_eth_tx_buffer(port_ids[0], 0, tx_bufs[0], pkts_burst[i]);
        }
    }

    for (auto const port_id : port_ids) {
        rte_eth_timesync_disable(port_id);

        if (rte_eth_dev_stop(port_id) != 0) {
            RTE_LOG(ERR, PORT, "Error stopping Ethernet device for port %u.\n", port_id);
        }

        rte_eth_dev_close(port_id);
    }

    rte_eal_cleanup();

    std::cout << "Exiting. Bye..." << std::endl;

    return EXIT_SUCCESS;
}
