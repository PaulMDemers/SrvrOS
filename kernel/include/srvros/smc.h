#ifndef SRVROS_SMC_H
#define SRVROS_SMC_H
#include <stdbool.h>
#include <srvros/smc_abi.h>
void smc_init(void);
bool smc_quiesce(void); /* Stop admitting reads and drain any in-flight transaction. */
void smc_status(struct srv_smc_status *);
void smc_print_status(void);
#endif
