#ifndef SRVROS_AHCI_H
#define SRVROS_AHCI_H

#include <stdbool.h>
#include <stdint.h>

bool ahci_init(void);
void ahci_print_status(void);
enum ahci_probe_result { AHCI_PROBE_REFUSED, AHCI_PROBE_FAILED, AHCI_PROBE_PASS, AHCI_PROBE_UNSAFE };
enum ahci_probe_result ahci_air61_probe(bool port_state, bool reset, bool partition);
enum ahci_probe_result ahci_air61_discovery(void);
enum ahci_probe_result ahci_air61_scratch(const uint8_t proof[64], bool verify, bool internal);

#endif
