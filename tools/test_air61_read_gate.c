/* Host boundary only; compile the same instance as the native object. */
#define SRVROS_ECAM_GATE_HOST_TEST
#define AIR61_READ_GATE_INSTANCE AIR61_READ_GATE_PASS
#include "../kernel/diagnostics/air61_read_gate_instance.c"
