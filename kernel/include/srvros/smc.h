#ifndef SRVROS_SMC_H
#define SRVROS_SMC_H
#include <srvros/smc_abi.h>
void smc_init(void);
void smc_status(struct srv_smc_status *);
void smc_print_status(void);
#endif
