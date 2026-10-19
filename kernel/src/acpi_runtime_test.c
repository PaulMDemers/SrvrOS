#include <srvros/acpi_runtime_test.h>
#include <srvros/console.h>
/* The full runtime adapter is linked only into the separate VM test kernel. */
__attribute__((weak)) void acpi_runtime_test_command(const char *args) {
    (void)args;
    console_write("acpi-runtime: unavailable in normal kernel; use the separate VM test build\n");
}
