#include "device.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void device_list_init(DeviceList *list) {
    list -> items = NULL;
    list -> count = 0;
    list -> capacity = 0;
}

void device_list_free(DeviceList *list) {
    free(list -> items);
}

Device *device_list_upsert(DeviceList *list, const char *ip, const char* mac) {
    Device *dev = device_list_find(list, mac);
    if(dev) {
        snprintf(dev -> ip, IP_STR_LEN, "%s", ip);
        dev -> last_seen = time(NULL);
        dev -> vendor = NULL;
        dev -> known = 0;
        return dev;
    }
    if(list -> count == list -> capacity) {
        size_t new_cap = list -> capacity == 0 ? 8 : list -> capacity * 2;
        Device *new_list = realloc(list -> items, sizeof(Device) * new_cap);
        if(!new_list){
            return NULL;
        }
        list -> items = new_list;
        list -> capacity = new_cap;
    }
    Device *new_device = &list -> items[list -> count];
    snprintf(new_device -> ip, IP_STR_LEN, "%s", ip);
    snprintf(new_device -> mac, MAC_STR_LEN, "%s", mac);
    new_device -> last_seen = time(NULL);
    new_device -> vendor = NULL;
    new_device -> known = 0;
    list -> count++;
    return new_device;
}

Device *device_list_find(DeviceList *list, const char *mac) {
    for(size_t i = 0; i < list -> count; i++) {
        if(strcmp(list -> items[i].mac, mac) == 0) {
            return &list -> items[i];
        }
    }
    return NULL;
}
