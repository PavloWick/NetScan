#include "scan.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netpacket/packet.h>
#include <pcap.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

static void set_error(char *errbuf, size_t errbuf_len, const char *message) {
    if(errbuf && errbuf_len > 0) {
        snprintf(errbuf, errbuf_len, "%s", message);
    }
}

static int find_interface_mac(const char *iface_name, uint8_t mac[6]) {
    struct ifaddrs *ifaddr;
    if(getifaddrs(&ifaddr) != 0) {
        return -1;
    }

    int found = -1;
    for(struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if(!ifa->ifa_addr || strcmp(ifa->ifa_name, iface_name) != 0) {
            continue;
        }
        if(ifa->ifa_addr->sa_family != AF_PACKET) {
            continue;
        }

        struct sockaddr_ll *packet_addr = (struct sockaddr_ll *)ifa->ifa_addr;
        if(packet_addr->sll_halen != 6) {
            continue;
        }

        memcpy(mac, packet_addr->sll_addr, 6);
        found = 0;
        break;
    }

    freeifaddrs(ifaddr);
    return found;
}

static int prefix_length(uint32_t netmask) {
    uint32_t mask = ntohl(netmask);
    int prefix = 0;

    while(mask & 0x80000000U) {
        prefix++;
        mask <<= 1;
    }

    return prefix;
}

static void print_scan_target(const ScanConfig *config) {
    struct in_addr network_addr;
    char network[INET_ADDRSTRLEN];

    network_addr.s_addr = config->ip & config->netmask;
    if(!inet_ntop(AF_INET, &network_addr, network, sizeof(network))) {
        snprintf(network, sizeof(network), "unknown");
    }

    printf(" Scanning interface %s on %s/%d\n",
           config->iface, network, prefix_length(config->netmask));
}

int scan_config_init(ScanConfig *config, const char *iface_name,
                     char *errbuf, size_t errbuf_len) {
    if(!config) {
        set_error(errbuf, errbuf_len, "scan configuration is missing");
        return -1;
    }

    struct ifaddrs *ifaddr;
    if(getifaddrs(&ifaddr) != 0) {
        if(errbuf && errbuf_len > 0) {
            snprintf(errbuf, errbuf_len,
                     "Failed to read network interfaces: %s",
                     strerror(errno));
        }
        return -1;
    }

    int found = -1;
    memset(config, 0, sizeof(*config));

    for(struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if(!ifa->ifa_addr || !ifa->ifa_netmask) {
            continue;
        }
        if(ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if((ifa->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }
        if(iface_name && strcmp(ifa->ifa_name, iface_name) != 0) {
            continue;
        }

        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        struct sockaddr_in *mask = (struct sockaddr_in *)ifa->ifa_netmask;
        snprintf(config->iface, sizeof(config->iface), "%s", ifa->ifa_name);
        config->ip = addr->sin_addr.s_addr;
        config->netmask = mask->sin_addr.s_addr;

        if(find_interface_mac(config->iface, config->mac) != 0) {
            continue;
        }

        found = 0;
        break;
    }

    freeifaddrs(ifaddr);

    if(found != 0) {
        if(iface_name) {
            if(errbuf && errbuf_len > 0) {
                snprintf(errbuf, errbuf_len,
                         "Interface '%s' was not found or has no IPv4 address/MAC.",
                         iface_name);
            }
        } else {
            set_error(errbuf, errbuf_len,
                      "No usable network interface found. Try selecting one with: netscan -i <interface>");
        }
        return -1;
    }

    return 0;
}

void build_arp_request(uint8_t *pkt,
                              const uint8_t src_mac[6],
                              uint32_t src_ip,
                              uint32_t target_ip) {
    struct ether_header *eth = (struct ether_header *)pkt;
    struct ether_arp *arp = (struct ether_arp *)(pkt + sizeof(*eth));
    memset(eth -> ether_dhost, 0xff, 6);
    memcpy(eth -> ether_shost, src_mac, 6);
    eth -> ether_type = htons(ETHERTYPE_ARP);
    arp -> ea_hdr.ar_hrd = htons(ARPHRD_ETHER); // (1) Ethernet
    arp -> ea_hdr.ar_pro = htons(ETHERTYPE_IP);
    arp -> ea_hdr.ar_hln = 6;
    arp -> ea_hdr.ar_pln = 4;
    arp -> ea_hdr.ar_op = htons(ARPOP_REQUEST); // (1) Operation Request
    memcpy(arp -> arp_sha, src_mac, 6);
    memcpy(arp -> arp_spa, &src_ip, 4);
    memset(arp -> arp_tha, 0x00, 6);
    memcpy(arp -> arp_tpa, &target_ip, 4);
}

int scan_with_arp(DeviceList *out, const ScanConfig *config, double *elapsed_seconds) {
    if(!config) {
        fprintf(stderr, "scan configuration is missing\n");
        return -1;
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle = pcap_open_live(config->iface, BUFSIZ, 0, 100, errbuf);
    if(!handle) {
        fprintf(stderr, "Unable to open interface '%s': %s\n",
                config->iface, errbuf);
        fprintf(stderr,
                "Packet capture usually requires elevated privileges. Try: sudo netscan -i %s\n",
                config->iface);
        return-1;
    }
    struct bpf_program fp;
    if(pcap_compile(handle, &fp, "arp and arp[6:2] = 2", 0, PCAP_NETMASK_UNKNOWN) == -1) {
        fprintf(stderr, "pcap_compile: %s\n", pcap_geterr(handle));
        pcap_close(handle);
        return -1;
    }
    if(pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "pcap_setfilter: %s\n", pcap_geterr(handle));
        pcap_freecode(&fp);
        pcap_close(handle);
        return -1;
    }
    pcap_freecode(&fp);

    struct timespec scan_start;
    struct timespec scan_end;
    clock_gettime(CLOCK_MONOTONIC, &scan_start);
    printf("\n\033[38;5;33m%35s\033[0m", "=== Building & Sending... ===\n");
    print_scan_target(config);

    uint32_t ip_host = ntohl(config->ip);
    uint32_t mask_host = ntohl(config->netmask);
    uint32_t network_host = ip_host & mask_host;
    uint32_t broadcast_host = network_host | ~mask_host;
    uint32_t host_count = broadcast_host - network_host - 1;

    if(host_count == 0) {
        fprintf(stderr, "interface '%s' has no scannable IPv4 host range\n",
                config->iface);
        pcap_close(handle);
        return -1;
    }

    if(host_count > 4096) {
        fprintf(stderr,
                "refusing to scan %u hosts on interface '%s'; choose a smaller subnet\n",
                host_count, config->iface);
        pcap_close(handle);
        return -1;
    }

    for(uint32_t target_host = network_host + 1; target_host < broadcast_host; target_host++) {
        if(target_host == ip_host) {
            continue;
        }

        uint32_t target_ip = htonl(target_host);
        uint8_t pkt[42];
        build_arp_request(pkt, config->mac, config->ip, target_ip);
        if(pcap_sendpacket(handle, pkt, sizeof(pkt)) != 0) {
            fprintf(stderr, "failed to send ARP request on '%s': %s\n",
                    config->iface, pcap_geterr(handle));
            pcap_close(handle);
            return -1;
        }
    }

    if(pcap_setnonblock(handle, 1, errbuf) == -1) {
        fprintf(stderr, "pcap_setnonblock: %s\n", errbuf);
        pcap_close(handle);
        return -1;
    }

    time_t deadline = time(NULL) + 1;
    while(time(NULL) < deadline) {
        struct pcap_pkthdr *hdr;
        const u_char *data;
        int rc = pcap_next_ex(handle, &hdr, &data);
        if(rc == 0) continue;
        if(rc < 0) break;
        if(hdr -> caplen < 42) continue;

        struct ether_arp *arp = (struct ether_arp *)(data + sizeof(struct ether_header));
        if(ntohs(arp -> ea_hdr.ar_op) != ARPOP_REPLY) continue;
        char ip_str[IP_STR_LEN];
        inet_ntop(AF_INET, arp -> arp_spa, ip_str, sizeof(ip_str));

        char mac_str[MAC_STR_LEN];
        snprintf(mac_str, sizeof(mac_str),
                 "%02x:%02x:%02x:%02x:%02x:%02x",
                 arp -> arp_sha[0], arp -> arp_sha[1], arp -> arp_sha[2],
                 arp -> arp_sha[3], arp -> arp_sha[4], arp -> arp_sha[5]);

        if(!device_list_upsert(out, ip_str, mac_str)) {
            fprintf(stderr, "failed to store discovered device\n");
            pcap_close(handle);
            return -1;
        }

    }
    clock_gettime(CLOCK_MONOTONIC, &scan_end);
    double elapsed = (scan_end.tv_sec - scan_start.tv_sec)
        + (scan_end.tv_nsec - scan_start.tv_nsec) / 1000000000.0;
    if(elapsed_seconds) {
        *elapsed_seconds = elapsed;
    }
    pcap_close(handle);
    return 0;
}
