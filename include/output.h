#ifndef OUTPUT_H_
#define OUTPUT_H_

#include "device.h"

void output_print_device_table(const DeviceList *device_list, int oui_loaded);
void output_print_elapsed(double elapsed_seconds);
int output_print_demo_scan(void);

#endif // OUTPUT_H_
