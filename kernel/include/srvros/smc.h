#ifndef SRVROS_SMC_H
#define SRVROS_SMC_H
#include <stdbool.h>
#include <stdint.h>
#include <srvros/smc_abi.h>
void smc_init(void);
void smc_battery_init(void);
/* Exclusive, read-only MSLD probe for the isolated Pro8,1 lid trial. */
bool smc_lid_probe_start(void);
bool smc_lid_probe_read(uint8_t *closed);
struct battery_service_status;
void smc_battery_status(struct battery_service_status *);
bool smc_quiesce(void); /* Stop admitting reads and drain any in-flight transaction. */
void smc_status(struct srv_smc_status *);
void smc_print_status(void);
#endif
