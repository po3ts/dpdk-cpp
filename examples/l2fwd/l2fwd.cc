#include <rte_cycles.h>
#include <rte_ip.h>
#include <rte_log.h>
#include <signal.h>

#include <cinttypes>
#include <iostream>

#include "dpdkpp/device_manager.hh"
#include "dpdkpp/rte_util.hh"
#include "ptpv1.hh"

static volatile bool force_quit;

static void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        std::printf("\n\nSignal %d received, preparing to exit...\n", signum);
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
            std::printf(PTP_SOURCE_UUID_PRT_FMT "[%04d]: %s.\n",
                        PTP_SOURCE_UUID_BYTES(ptp_hdr->sourceUuid),
                        rte_be_to_cpu_16(ptp_hdr->sequenceId),
                        PTP_CONTROL_FIELD_STRING(ptp_hdr->control));
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "This is a simple layer 2 forwarding application built around dpdk++."
              << std::endl;

    assert(dpdkpp::device_manager_t::eal_initialize(argc, argv));

    auto device_manager = dpdkpp::device_manager_t::create<4095U, 273U>("mbuf_pool");

    assert(device_manager->update_device_list());

    dpdkpp::device_t* device2 = device_manager->get_device_by_pcie_address("0000:02:00.2");
    assert(device2 != nullptr);

    dpdkpp::device_t* device3 = device_manager->get_device_by_pcie_address("0000:02:00.3");
    assert(device3 != nullptr);

    for (dpdkpp::device_t* device : {device2, device3}) {
        /*
            Display device information
        */
        std::cout << "Found device with port ID " << device->get_port_id();

        std::optional<struct rte_ether_addr> mac_addr = device->get_mac_address();
        if (mac_addr.has_value()) {
            std::cout << " and MAC address " << mac_addr.value();
        }

        std::cout << " at PCIe address " << device->get_pcie_address() << "." << std::endl;

        /*
            Get device info
        */
        struct rte_eth_dev_info dev_info;
        assert(device->invoke<rte_eth_dev_info_get>(&dev_info) == 0);

        /*
            Create port configuration and configure device
        */
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

        assert(device->configure(1, 1, &port_conf));

        /*
            Set up Rx and Tx queues
        */
        struct rte_eth_rxconf rx_conf = dev_info.default_rxconf;
        rx_conf.offloads = port_conf.rxmode.offloads;

        struct rte_eth_txconf tx_conf = dev_info.default_txconf;
        tx_conf.offloads = port_conf.txmode.offloads;

        assert(device->setup_rx_tx_queues(1024, 1024, &rx_conf, &tx_conf));

        /*
            Set up Tx buffers
        */
        assert(device->setup_tx_buffers(32));

        /*
            Start the device
        */
        assert(device->start());

        /*
            Enable PTP timestamping and promiscuous mode
        */
        assert(device->invoke<rte_eth_timesync_enable>() == 0);
        assert(device->invoke<rte_eth_promiscuous_enable>() == 0);
    }

    force_quit = false;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    uint16_t nb_rx;
    struct rte_mbuf* pkts_burst[32];

    // Print stats every second
    const uint64_t print_stats_period = rte_get_tsc_hz();
    uint64_t tsc_current = 0;
    uint64_t tsc_last = 0;

    auto print_stats = [](const dpdkpp::device_t* device) {
        std::printf("[%s] Rx: %7" PRIu64 " | Tx: %7" PRIu64 " | Dropped: %7" PRIu64 "\n",
                    device->get_pcie_address(),
                    device->get_nb_packets_received(0),
                    device->get_nb_packets_transmitted(0),
                    device->get_nb_packets_dropped(0));
    };

    while (likely(!force_quit)) {
        // Port 0 -> port 1
        device3->tx_buffer_flush(0);

        nb_rx = device2->rx_burst(0, pkts_burst, 32);

        for (uint16_t i = 0; i < nb_rx; ++i) {
            parse_ptpv1_header(pkts_burst[i]);
            device3->tx_buffer(0, pkts_burst[i]);
        }

        // Port 1 -> port 0
        device2->tx_buffer_flush(0);

        nb_rx = device3->rx_burst(0, pkts_burst, 32);

        for (uint16_t i = 0; i < nb_rx; ++i) {
            parse_ptpv1_header(pkts_burst[i]);
            device2->tx_buffer(0, pkts_burst[i]);
        }

        tsc_current = rte_rdtsc();
        if (tsc_current - tsc_last >= print_stats_period) {
            tsc_last = tsc_current;
            print_stats(device2);
            print_stats(device3);
        }
    }

    assert(device2->invoke<rte_eth_timesync_disable>() == 0);
    assert(device3->invoke<rte_eth_timesync_disable>() == 0);

    assert(device2->stop());
    assert(device3->stop());

    device_manager.reset();

    dpdkpp::device_manager_t::eal_cleanup();

    std::cout << "Exiting. Bye..." << std::endl;

    return EXIT_SUCCESS;
}
