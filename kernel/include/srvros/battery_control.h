#ifndef SRVROS_BATTERY_CONTROL_H
#define SRVROS_BATTERY_CONTROL_H
#include <srvros/battery_control_abi.h>
#include <srvros/battery_service.h>
/* Pure mailbox policy. Caller serializes access; no allocation, waits or IO. */
struct battery_control_mailbox {
    uint64_t group,serial,expected_epoch;
    unsigned operation,dispatched;
    int result;
};
bool battery_control_bind(struct battery_control_mailbox *,uint64_t group);
void battery_control_revoke(struct battery_control_mailbox *);
int battery_control_enqueue(struct battery_control_mailbox *,uint64_t group,unsigned operation,uint64_t expected_epoch,const struct battery_service_status *);
struct srv_battery_control battery_control_view(const struct battery_control_mailbox *,uint64_t group,uint64_t epoch);
unsigned battery_control_take(struct battery_control_mailbox *);
void battery_control_complete(struct battery_control_mailbox *,int result);
/* Strong runtime implementation only grants a scoped diagnostic owner. */
int battery_control_request(uint64_t group,unsigned operation,uint64_t epoch,struct srv_battery_control *out);
#endif
