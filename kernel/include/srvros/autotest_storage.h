#ifndef SRVROS_AUTOTEST_STORAGE_H
#define SRVROS_AUTOTEST_STORAGE_H
#include <stdbool.h>
struct block_device;
bool autotest_storage_run(struct block_device *device, const char *run);
#endif
