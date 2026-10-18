#ifndef SRVROS_SMC_TRANSPORT_H
#define SRVROS_SMC_TRANSPORT_H
#include <stdbool.h>
#include <stdint.h>
/* Read operations only. The caller owns/serializes the controller. No reset,
 * data draining, key writes, fan policy or port selection is implemented here. */
struct smc_io { void *context;uint8_t (*status)(void *);uint8_t (*read)(void *);void (*command)(void *,uint8_t);void (*argument)(void *,uint8_t); };
enum smc_phase { SMC_IDLE,SMC_START,SMC_INPUT_OPEN,SMC_INPUT_BUSY,SMC_RESULT,SMC_FINISH,SMC_DONE,SMC_FAILED };
enum smc_error { SMC_OK,SMC_DEADLINE,SMC_CLOCK,SMC_POLL_LIMIT,SMC_EXTRA_DATA,SMC_CANCELED,SMC_NO_RESULT };
struct smc_transport {
    struct smc_io io;enum smc_phase phase,failed_phase;enum smc_error error;
    uint64_t start_us,last_us;unsigned polls;uint8_t command,key[4],length,index,received,status,result[32];bool poisoned;
};
void smc_transport_init(struct smc_transport *,struct smc_io);
bool smc_transport_begin(struct smc_transport *,bool metadata,const char key[4],unsigned bytes,uint64_t now);
/* Each poll performs at most one status read and one data/command access.
 * Input-open and busy checks intentionally use distinct status observations. */
int smc_transport_poll(struct smc_transport *,uint64_t now);
void smc_transport_cancel(struct smc_transport *);
uint32_t smc_fan_millirpm(const uint8_t bytes[2]);
#endif
