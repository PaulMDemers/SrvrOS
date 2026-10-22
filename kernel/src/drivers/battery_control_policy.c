#include <srvros/battery_control.h>
bool battery_control_bind(struct battery_control_mailbox *m,uint64_t group) {
    if(!group || m->group || m->operation)return false;
    m->group=group;m->result=0;return true;
}
void battery_control_revoke(struct battery_control_mailbox *m) {
    m->group=0;
    if(m->operation)m->result=SRV_BATTERY_CONTROL_CANCELLED;
    m->operation=0;m->dispatched=0;
}
int battery_control_enqueue(struct battery_control_mailbox *m,uint64_t group,unsigned op,uint64_t epoch,const struct battery_service_status *s) {
    if(op>SRV_BATTERY_STOP)return SRV_BATTERY_CONTROL_INVALID;
    if(!op)return 0;
    if(!group || group!=m->group)return SRV_BATTERY_CONTROL_UNAVAILABLE;
    if(m->operation)return SRV_BATTERY_CONTROL_BUSY;
    if(epoch!=s->epoch)return SRV_BATTERY_CONTROL_STALE;
    if(op==SRV_BATTERY_START && s->state!=BATTERY_SERVICE_DISABLED && s->state!=BATTERY_SERVICE_STOPPED && s->state!=BATTERY_SERVICE_UNAVAILABLE)return SRV_BATTERY_CONTROL_STATE;
    if(op==SRV_BATTERY_STOP && s->state!=BATTERY_SERVICE_STARTING && s->state!=BATTERY_SERVICE_ACTIVE)return SRV_BATTERY_CONTROL_STATE;
    if(m->serial==UINT64_MAX)return SRV_BATTERY_CONTROL_FAILED;
    m->serial++;m->operation=op;m->expected_epoch=epoch;m->dispatched=0;m->result=SRV_BATTERY_CONTROL_QUEUED;
    return 0;
}
struct srv_battery_control battery_control_view(const struct battery_control_mailbox *m,uint64_t group,uint64_t epoch) {
    bool allowed=group && group==m->group;
    return (struct srv_battery_control){.version=SRV_BATTERY_CONTROL_VERSION,.size=sizeof(struct srv_battery_control),
        .available=allowed,.pending=allowed?m->operation:0,.serial=allowed?m->serial:0,.epoch=epoch,
        .result=allowed?m->result:SRV_BATTERY_CONTROL_UNAVAILABLE};
}
unsigned battery_control_take(struct battery_control_mailbox *m) {
    if(!m->group || !m->operation || m->dispatched)return 0;
    m->dispatched=1;return m->operation;
}
void battery_control_complete(struct battery_control_mailbox *m,int result) {
    if(!m->operation || !m->dispatched)return;
    m->operation=0;m->dispatched=0;m->result=result;
}
