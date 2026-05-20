#include "device.h"
#include "oui.h"
#include "scan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NETSCAN_OUI_PATH
#define NETSCAN_OUI_PATH "data/manuf"
#endif

static void print_usage(FILE *stream) {
    fprintf(stream,
            "Usage: netscan [options]\n"
            "\n"
            "Options:\n"
            "  -i <interface>   Scan with a specific network interface\n"
            "  -h, --help       Show this help message\n"
            "\n"
            "Examples:\n"
            "  sudo netscan\n"
            "  sudo netscan -i wlp0s20f3\n");
}

int main(int argc, char **argv) {
    const char *iface_name = NULL;

    if(argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_usage(stdout);
        return 0;
    } else if(argc == 3 && strcmp(argv[1], "-i") == 0) {
        iface_name = argv[2];
    } else if(argc != 1) {
        print_usage(stderr);
        return 1;
    }

    DeviceList device_list;
    device_list_init(&device_list);
    double elapsed_seconds = 0.0;
    int oui_loaded = oui_load(NETSCAN_OUI_PATH) == 0;

    if(!oui_loaded && strcmp(NETSCAN_OUI_PATH, "data/manuf") != 0) {
        oui_loaded = oui_load("data/manuf") == 0;
    }

    ScanConfig config;
    char scan_error[256];
    if(scan_config_init(&config, iface_name, scan_error, sizeof(scan_error)) != 0) {
        fprintf(stderr, "%s\n", scan_error);
        fprintf(stderr, "List interfaces with: ip addr\n");
        oui_free();
        device_list_free(&device_list);
        return 1;
    }

    if(scan_with_arp(&device_list, &config, &elapsed_seconds) != 0) {
        fprintf(stderr, "scan failed\n");
        oui_free();
        device_list_free(&device_list);
        return 1;
    }

    printf(" Found %zu devices:\n", device_list.count);
    printf("  \033[97m%-15s  %-17s  %s\033[0m\n", "IP", "MAC", "Device Type");

    for(size_t i = 0; i < device_list.count; i++) {
        const char *vendor = oui_loaded ? oui_lookup(device_list.items[i].mac) : NULL;
        printf("  %-15s  %-17s  ", device_list.items[i].ip,
               device_list.items[i].mac);
        if(vendor) {
            printf("%s\n", vendor);
        } else {
            printf("\033[91mUnknown\033[0m\n");
        }
    }
    char elapsed_str[64];
    snprintf(elapsed_str, sizeof(elapsed_str), "=== Completed: %.3f seconds ===\n", elapsed_seconds);
    printf("\033[38;5;33m%35s\033[0m\n", elapsed_str);
    oui_free();
    device_list_free(&device_list);

    return 0;
}
