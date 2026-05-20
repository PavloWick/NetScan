#include "device.h"
#include "oui.h"
#include "output.h"
#include "scan.h"
#include <stdio.h>
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
            "  --demo           Print sample scan output without using the network\n"
            "  -h, --help       Show this help message\n"
            "\n"
            "Examples:\n"
            "  sudo netscan\n"
            "  sudo netscan -i wlp0s20f3\n"
            "  netscan --demo\n");
}

int main(int argc, char **argv) {
    const char *iface_name = NULL;

    if(argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_usage(stdout);
        return 0;
    } else if(argc == 2 && strcmp(argv[1], "--demo") == 0) {
        return output_print_demo_scan();
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

    output_print_device_table(&device_list, oui_loaded);
    output_print_elapsed(elapsed_seconds);
    oui_free();
    device_list_free(&device_list);

    return 0;
}
