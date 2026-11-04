#ifndef SRVROS_INSTALLED_ROOT_H
#define SRVROS_INSTALLED_ROOT_H
#include <srvros/block.h>
enum installed_root_result {
    INSTALLED_ROOT_LEGACY, INSTALLED_ROOT_SELECTED, INSTALLED_ROOT_INVALID
};
/* Call after all raw media registration and before any mount. Opt-in requires
 * every registered raw medium to have a valid primary GPT. No disk writes.
 * reason, when supplied, receives a stable literal on failure. */
enum installed_root_result installed_root_select(const char *cmdline,
    struct block_device **out, const char **reason);
#endif
