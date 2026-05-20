#include "output.h"

#include "oui.h"

#include <stdio.h>

void output_print_device_table(const DeviceList *device_list, int oui_loaded) {
    printf(" Found %zu devices:\n", device_list->count);
    printf("  \033[97m%-15s  %-17s  %s\033[0m\n", "IP", "MAC", "Device Type");

    for(size_t i = 0; i < device_list->count; i++) {
        const char *vendor = device_list->items[i].vendor;
        if(!vendor && oui_loaded) {
            vendor = oui_lookup(device_list->items[i].mac);
        }

        printf("  %-15s  %-17s  ", device_list->items[i].ip,
               device_list->items[i].mac);
        if(vendor) {
            printf("%s\n", vendor);
        } else {
            printf("\033[91mUnknown\033[0m\n");
        }
    }
}

void output_print_elapsed(double elapsed_seconds) {
    char elapsed_str[64];
    snprintf(elapsed_str, sizeof(elapsed_str),
             "=== Completed: %.3f seconds ===\n", elapsed_seconds);
    printf("\033[38;5;33m%35s\033[0m\n", elapsed_str);
}

static int add_demo_device(DeviceList *device_list,
                           const char *ip,
                           const char *mac,
                           const char *vendor) {
    Device *device = device_list_upsert(device_list, ip, mac);
    if(!device) {
        return -1;
    }

    device->vendor = vendor;
    return 0;
}

int output_print_demo_scan(void) {
    DeviceList demo_devices;
    device_list_init(&demo_devices);

    if(add_demo_device(&demo_devices, "192.168.1.1", "00:11:22:33:44:55", "Example Networks") != 0 ||
       add_demo_device(&demo_devices, "192.168.1.14", "8c:85:90:12:34:56", "Apple, Inc.") != 0 ||
       add_demo_device(&demo_devices, "192.168.1.22", "b8:27:eb:aa:bb:cc", "Raspberry Pi Foundation") != 0 ||
       add_demo_device(&demo_devices, "192.168.1.31", "3c:5a:b4:44:55:66", "Google, Inc.") != 0 ||
       add_demo_device(&demo_devices, "192.168.1.42", "f4:f5:d8:77:88:99", NULL) != 0) {
        fprintf(stderr, "failed to build demo output\n");
        device_list_free(&demo_devices);
        return 1;
    }

    printf("\n\033[38;5;33m%35s\033[0m", "=== Building & Sending... ===\n");
    printf(" Scanning interface demo0 on 192.168.1.0/24\n");
    output_print_device_table(&demo_devices, 0);
    printf("\033[38;5;33m%35s\033[0m\n", "=== Completed: 0.842 seconds ===\n");

    device_list_free(&demo_devices);
    return 0;
}
