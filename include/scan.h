#ifndef SCAN_H_
#define SCAN_H_

#include <stdint.h>
#include <stddef.h>

#include "device.h"

#define IFACE_NAME_LEN 64

typedef struct {
    char iface[IFACE_NAME_LEN];
    uint8_t mac[6];
    uint32_t ip;
    uint32_t netmask;
} ScanConfig;

int scan_config_init(ScanConfig *config, const char *iface_name,
                     char *errbuf, size_t errbuf_len);
int scan_with_arp(DeviceList *out, const ScanConfig *config,
                  double *elapsed_seconds);

void build_arp_request(uint8_t *pkt,
                              const uint8_t src_mac[6],
                              uint32_t src_ip,
                              uint32_t target_ip);

#endif // SCAN_H_
