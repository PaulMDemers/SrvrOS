#pragma once
#include <stdint.h>
#include <stdbool.h>
#define HFW_MAP_CAP 128u
#define HFW_DMAR_BYTES 136u
struct hfw_entry {uint64_t base,length,type;};
struct hfw_sample {
 uint32_t identity[5]; unsigned count;
 struct hfw_entry entries[HFW_MAP_CAP];
 uint8_t dmar[HFW_DMAR_BYTES]; uint64_t table_pa,begin,end;
};
struct hfw_result {unsigned status;struct hfw_sample sample[2];};
struct hfw_io {
 void *ctx;
 bool (*identity)(void *,uint32_t [5]);
 bool (*catalog)(void *,struct hfw_entry *,unsigned *);
 bool (*table)(void *,const struct hfw_entry *,unsigned,uint8_t *,uint64_t *);
 uint64_t (*now)(void *);
 void (*progress)(void *,unsigned);
};
/* Pure bounded firmware-copy validation; no map/register/writer API. */
void hfw_sha256(const uint8_t [HFW_DMAR_BYTES],uint8_t [32]);
bool hfw_catalog_valid(const struct hfw_entry *,unsigned);
bool hfw_table_covered(const struct hfw_entry *,unsigned,uint64_t);
bool hfw_capture(struct hfw_result *,const struct hfw_io *,const uint8_t [32]);
void hfw_report(const struct hfw_result *);
bool haswell_firmware_run(void);
