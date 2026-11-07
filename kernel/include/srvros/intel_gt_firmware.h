#ifndef SRVROS_INTEL_GT_FIRMWARE_H
#define SRVROS_INTEL_GT_FIRMWARE_H
#include <stdbool.h>
#include <stdint.h>
#define INTEL_FW_WORDS 13u
extern const uint32_t intel_fw_offsets[INTEL_FW_WORDS];
enum intel_fw_status {IFW_REFUSED,IFW_OBSERVED,IFW_ACCESS,IFW_READ,IFW_CLOCK,IFW_CHANGED,IFW_RETAINED};
struct intel_fw_endpoint {uint64_t base,length,type;uint32_t asls,swsci;};
struct intel_fw_sample {uint64_t begin_us,end_us;uint32_t words[INTEL_FW_WORDS],reads;};
struct intel_fw_result {uint32_t status,maps,retained;struct intel_fw_endpoint endpoint;struct intel_fw_sample sample[2];};
/* Only the fixed header and advertised v1/v2 public readiness/command words.
 * No mailbox calls, writes, VBT/EDID copy, firmware retirement or wake authority.
 * map(false) must leave no alias; an uncertain partial map is retained by adapter. */
struct intel_fw_io {
 void *context;
 bool (*guard)(void *);
 bool (*endpoint)(void *,struct intel_fw_endpoint *);
 bool (*map)(void *,uint64_t);
 bool (*read)(void *,uint32_t,uint32_t *);
 bool (*unmap)(void *);
 uint64_t (*now)(void *);
};
bool intel_fw_observe(struct intel_fw_result *,const struct intel_fw_io *);
bool intel_gt_firmware_probe_report(void);
#endif
