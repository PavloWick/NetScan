#ifndef DEVICE_H_
#define DEVICE_H_

#include <stddef.h> // size_t
#include <time.h>

#define IP_STR_LEN 16
#define MAC_STR_LEN 18

typedef struct {
    char ip[IP_STR_LEN];
    char mac[MAC_STR_LEN];
    const char *vendor;
    int known;
    time_t last_seen;
} Device;

typedef struct {
    Device *items;
    size_t count;
    size_t capacity;
} DeviceList;

void device_list_init(DeviceList *list);
void device_list_free(DeviceList *list);

Device *device_list_upsert(DeviceList *list, const char *ip, const char *mac);
Device *device_list_find(DeviceList *list, const char *mac);

#endif // DEVICE_H_
