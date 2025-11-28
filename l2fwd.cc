#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_pci.h>

#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "This is a simple layer 2 forwarding application built around DPDK." << std::endl;

    if (rte_eal_init(argc, argv) < 0) {
        rte_exit(EXIT_FAILURE, "Cannot initialize Environment Abstraction Layer (EAL).\n");
    }

    if (rte_eth_dev_count_avail() == 0) {
        rte_exit(EXIT_FAILURE, "No Ethernet devices available.\n");
    }

    char const* dev_name[] = {"0000:02:00.0", "0000:02:00.1"};
    size_t const dev_count = sizeof(dev_name) / sizeof(dev_name[0]);
    uint16_t port_id[dev_count];

    for (int i = 0; i < dev_count; ++i) {
        if (rte_eth_dev_get_port_by_name(dev_name[i], &port_id[i]) < 0) {
            rte_exit(EXIT_FAILURE, "Device %s not found.\n", dev_name[i]);
        }
    }

    rte_eal_cleanup();

    std::cout << "Exiting. Bye..." << std::endl;

    return EXIT_SUCCESS;
}
