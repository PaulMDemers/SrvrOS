#ifndef SRVROS_WIFI_SERVICE_H
#define SRVROS_WIFI_SERVICE_H
#include <srvros/mt76_selection.h>
#include <srvros/wifi_abi.h>
/* CPU0 serialized calls, no scheduler/blocking/reentry. Syscalls hold IRQs off;
 * initial capture publication must use the same serialization. Presence is a
 * bounded, read-only check: never USB control, polling, allocation or RF work.
 * Context/label must live until remove, including failed refreshes. */
struct wifi_service_provider {
    void *context;
    const char *label;
    bool (*present)(void *);
};
/* Register only after complete native cleanup/ownership proof. An owner ID is
 * never reused during this boot, including after remove/re-register. */
uint64_t wifi_service_publish(const struct wifi_service_provider *,
    const struct mt76_scan_batch *,const struct mt76_selection_io *);
bool wifi_service_refresh(uint64_t,const struct mt76_scan_batch *,const struct mt76_selection_io *);
void wifi_service_remove(uint64_t);
int wifi_service_snapshot(uint64_t,struct srv_wifi_snapshot *);
/* Trial UI admission only; does not accept discovery or authorize RF work. */
bool wifi_service_trial_ready(int,const struct srv_wifi_snapshot *,bool allow_empty);
int wifi_service_select(const struct srv_wifi_select *);
/* A native owner may publish its already validated fixed-channel peer. The
 * syscall only queues an operation. Only the original owner can consume and
 * finish it after its actual RF/IP lifecycle reaches the corresponding state. */
struct mt76_peer5;
struct wifi_service_command { uint64_t serial; unsigned operation; };
uint64_t wifi_service_publish_connection(const struct wifi_service_provider *,const struct mt76_peer5 *);
int wifi_service_request(unsigned operation,const struct srv_wifi_select *);
bool wifi_service_take(uint64_t,void *,struct wifi_service_command *);
bool wifi_service_finish(uint64_t,void *,const struct wifi_service_command *,bool success);
/* Complete a proven disconnect and retire requests from the previous view. */
bool wifi_service_rearm_connection(uint64_t,void *,const struct wifi_service_command *,uint64_t next_generation);
void wifi_service_seal_connection(uint64_t,void *,bool success);
struct wifi_service_counts { uint64_t published,reads,selected,stale,refused,removed; };
/* Anonymous boot-lifetime totals, saturating; same serialized CPU0 contract. */
void wifi_service_counts(struct wifi_service_counts *);
#endif
