#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Diagnostic software model only. No I/O/writer callback or hardware authority.
 * Zero-initialize objects/pool and use one non-reset boot pool. Caller serializes
 * all access on CPU0; no SMP/interrupt-safe ownership claim. */
#define DISPLAY_OWNER_SLOTS 8u
#define DISPLAY_OWNER_WORDS 96u
#define DISPLAY_OWNER_REQUIRED 127u
#define DISPLAY_OWNER_BACKING 1u
#define DISPLAY_OWNER_USERS 2u
#define DISPLAY_OWNER_HDA 4u
#define DISPLAY_OWNER_IRQ 8u
#define DISPLAY_OWNER_RESTORE 16u
#define DISPLAY_OWNER_RECOVERY 32u
#define DISPLAY_OWNER_FIRMWARE 64u
/* Assurance bits: backing lifetime, domain users, HDA ownership, IRQ ownership,
 * restore target, independent recovery, firmware exclusion. Observations alone
 * must not set any of these. All unknown bits remain refusal reasons. */
enum display_owner_state {DISPLAY_OWNER_EMPTY,DISPLAY_OWNER_OBSERVED,
    DISPLAY_OWNER_ADOPTED,DISPLAY_OWNER_CLOSING,DISPLAY_OWNER_RETIRED,DISPLAY_OWNER_FAULT};
enum display_owner_domain {DISPLAY_OWNER_ROUTE,DISPLAY_OWNER_AUDIO};
enum display_owner_reason {DISPLAY_OWNER_ACTIVE=128,DISPLAY_OWNER_CHANGED=256,
    DISPLAY_OWNER_PROFILE=512,DISPLAY_OWNER_REFS=1024,DISPLAY_OWNER_INFLIGHT=2048,
    DISPLAY_OWNER_STATE=4096,DISPLAY_OWNER_POISON=8192,DISPLAY_OWNER_STALE=16384};
struct display_owner_profile {
    uint32_t gpu,pch,hda,subsystem,assured,active;
    bool access_valid;
    uint32_t raw[DISPLAY_OWNER_WORDS];
};
struct display_owner_pool {uint64_t epoch;};
struct display_owner;
struct display_owner_token {const struct display_owner *owner;uint64_t epoch,serial;unsigned slot;};
struct display_owner_ref {uint64_t serial;unsigned domain;bool live,busy;};
struct display_owner {
    struct display_owner_profile profile;
    uint64_t epoch,serial;
    enum display_owner_state state;
    unsigned refs[2],inflight,guard;
    uint32_t fault;
    struct display_owner_ref slots[DISPLAY_OWNER_SLOTS];
};
bool display_owner_open(struct display_owner *,struct display_owner_pool *,const struct display_owner_profile *);
bool display_owner_hold(struct display_owner *,uint64_t,unsigned,struct display_owner_token *);
bool display_owner_begin(struct display_owner *,const struct display_owner_token *);
bool display_owner_end(struct display_owner *,const struct display_owner_token *);
bool display_owner_release(struct display_owner *,const struct display_owner_token *);
bool display_owner_close(struct display_owner *,uint64_t);
bool display_owner_fault(struct display_owner *,uint64_t,uint32_t);
uint32_t display_owner_check(const struct display_owner *,uint64_t,const struct display_owner_profile *);
