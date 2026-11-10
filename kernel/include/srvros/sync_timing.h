#ifndef SRVROS_SYNC_TIMING_H
#define SRVROS_SYNC_TIMING_H
#include <stdint.h>
#include <stdbool.h>
#include <srvros/usb_storage.h>
#define SYNC_TIMING_RECORDS 8u
#define SYNC_CONTENT_ROLES 6u
/* Exclusive roles: directory/flags, allocation/free, root payload, root prefix
 * proof, nested sidecar work, nested sidecar equality proof. */
struct sync_content_io {
    uint64_t reads,read_bytes,read_us,writes,write_bytes,write_us,flushes,flush_us,failures;
};
struct sync_content_call {
    uint64_t slot,identity,inode,size,start_us,end_us,ok,dirty_after;
    struct sync_content_io io[SYNC_CONTENT_ROLES];
    struct usb_storage_content_timing commands;
};
struct sync_content_summary {
    uint64_t calls,bytes,failures;
    struct sync_content_call worst;
};
struct sync_timing_call {
    uint64_t start_us,files_end_us,end_us,file_failed,device_failed;
    struct usb_storage_timing usb[2];
    struct sync_content_summary content;
};
struct sync_timing_record {
    uint64_t pid,calls,total_us,files_us,devices_us,errors,conflicts,invalid;
    struct sync_timing_call worst;
};
struct sync_timing_snapshot {
    uint64_t enabled,active,invalid,dropped;
    struct sync_timing_record records[SYNC_TIMING_RECORDS];
};
/* CPU0 opt-in software wall observations only; no transfers, allocation,
 * logging, scheduler/flush policy or userspace ABI. Records last until reboot. */
bool sync_timing_enable(void);
bool sync_timing_enabled(void);
bool sync_timing_begin(uint64_t pid);
void sync_timing_files_done(bool ok);
void sync_timing_end(bool ok);
void sync_timing_snapshot(struct sync_timing_snapshot *out);
/* Host fixtures unrelated to opt-in content observation need no timing runtime. */
#if (defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_EXFAT_HOST_TEST) || defined(SRVROS_SYNC_CONTENT_DISABLED_HOST_TEST)) && !defined(SRVROS_SYNC_CONTENT_HOST_TEST)
static inline void sync_timing_content_fault(void) {}
static inline unsigned sync_timing_content_role_begin(unsigned role) { (void)role;return ~0u; }
static inline void sync_timing_content_role_end(unsigned token) { (void)token; }
static inline uint64_t sync_timing_content_identity(void) { return 0; }
static inline bool sync_timing_content_begin(uint64_t slot,uint64_t identity,uint64_t inode,uint64_t size) {
    (void)slot;(void)identity;(void)inode;(void)size;return false;
}
static inline void sync_timing_content_end(bool ok,bool dirty_after) { (void)ok;(void)dirty_after; }
static inline void sync_timing_content_io(unsigned role,unsigned kind,uint64_t bytes,uint64_t us,bool ok) {
    (void)role;(void)kind;(void)bytes;(void)us;(void)ok;
}
#else
void sync_timing_content_fault(void);
unsigned sync_timing_content_role_begin(unsigned role);
void sync_timing_content_role_end(unsigned token);
uint64_t sync_timing_content_identity(void);
bool sync_timing_content_begin(uint64_t slot,uint64_t identity,uint64_t inode,uint64_t size);
void sync_timing_content_end(bool ok,bool dirty_after);
/* kind0=uncached block read,1=block write,2=owner flush; supplied elapsed wall,
 * no extra controller access or clock read. Counters include failed attempts. */
void sync_timing_content_io(unsigned role,unsigned kind,uint64_t bytes,uint64_t us,bool ok);
#endif
#endif
