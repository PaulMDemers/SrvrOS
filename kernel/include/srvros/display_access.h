#ifndef SRVROS_DISPLAY_ACCESS_H
#define SRVROS_DISPLAY_ACCESS_H
#include <stdbool.h>
#include <stdint.h>
/* Participating CPU clients only. Not firmware/GT/DMA ownership. Lease control
 * is boot CPU serialized, before desktop clients/APs. Entries cover preemption
 * of a client on that CPU; no waits, resets or forced cancellation. */
struct display_access_token {uint64_t epoch, owner;};
bool display_access_enter(void);
void display_access_leave(void);
bool display_access_allowed(void);
bool display_access_acquire(uint64_t owner,struct display_access_token *);
bool display_access_held(const struct display_access_token *);
bool display_access_pin(const struct display_access_token *);
bool display_access_unpin(const struct display_access_token *);
/* Dirty/uncertain retirement stays blocked until machine reset. Wrong tokens
 * cannot release, and outstanding range pins prevent even a clean release. */
bool display_access_release(const struct display_access_token *,bool clean);
#endif
