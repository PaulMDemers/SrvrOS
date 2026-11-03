#ifndef SRVROS_SCANOUT_LIFETIME_H
#define SRVROS_SCANOUT_LIFETIME_H
#include <stdint.h>
#include <stdbool.h>
/* CPU0 serialized mechanism. Hardware adapters must supply their own validated
 * reservation and matching-completion proofs. No MMIO or allocator callback. */
#define SCANOUT_NONE 3u
#define SCANOUT_DEADLINE_US 100000u
#define SCANOUT_ASSURED_ALL 63u
#define SCANOUT_RAM_PIN 1u
#define SCANOUT_GGTT_PIN 2u
#define SCANOUT_DMA 4u
#define SCANOUT_ROUTE 8u
#define SCANOUT_RESTORE 16u
#define SCANOUT_FIRMWARE 32u
enum scanout_mode {SCANOUT_SHADOW=1,SCANOUT_HARDWARE=2};
enum scanout_state {SCANOUT_EMPTY,SCANOUT_ACTIVE,SCANOUT_CLOSING,SCANOUT_RETIRED,SCANOUT_FAULT};
enum scanout_phase {SCANOUT_NO_REQUEST,SCANOUT_SUBMITTING,SCANOUT_PENDING};
enum scanout_issue {SCANOUT_REJECTED,SCANOUT_ISSUED,SCANOUT_UNCERTAIN};
enum scanout_fault {SCANOUT_TIMEOUT=1,SCANOUT_CLOCK=2,SCANOUT_CHANGED=4,SCANOUT_PUBLICATION=8};
struct scanout_profile {uint64_t physical[3],bytes;uint32_t width,height,pitch,assured,mode;};
struct scanout_pool {uint64_t epoch;};
struct scanout_lifetime;
struct scanout_token {const struct scanout_lifetime *object;uint64_t epoch,serial;uint32_t target,mode;};
struct scanout_lifetime {
    struct scanout_profile profile;
    uint64_t epoch,owner,serial,copy_serial,request_serial,last_us,deadline_us;
    uint32_t state,phase,front,copy,ready,target,released,fault;
};
bool scanout_open(struct scanout_lifetime *,struct scanout_pool *,const struct scanout_profile *,uint64_t owner,uint64_t now);
bool scanout_copy_begin(struct scanout_lifetime *,uint64_t owner,struct scanout_token *);
bool scanout_copy_end(struct scanout_lifetime *,const struct scanout_token *,bool success);
bool scanout_request(struct scanout_lifetime *,uint64_t owner,uint64_t now,struct scanout_token *);
/* Call after publication: REJECTED is legal only with proof of zero writes.
 * Once a write may have occurred use UNCERTAIN, retaining every owned buffer. */
bool scanout_issued(struct scanout_lifetime *,const struct scanout_token *,enum scanout_issue,uint64_t now);
/* A matching identity is necessary, not sufficient hardware proof. Only an
 * audited adapter may form a hardware completion after retirement is proven. */
bool scanout_complete(struct scanout_lifetime *,const struct scanout_token *,uint32_t observed_target,uint32_t mode,uint64_t now);
bool scanout_close(struct scanout_lifetime *,uint64_t owner,uint64_t epoch);
bool scanout_tick(struct scanout_lifetime *,uint64_t now);
bool scanout_poison(struct scanout_lifetime *,uint32_t reason);
/* Claims each RAM release exactly once. Never releases the borrowed target0. */
bool scanout_take_release(struct scanout_lifetime *,unsigned target,uint64_t *physical);
bool scanout_invariant(const struct scanout_lifetime *);
#endif
