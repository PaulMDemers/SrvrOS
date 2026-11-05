#pragma once
#include <stdbool.h>
#include <stdint.h>
#define HAC_TABLE_CAP 64u
#define HAC_DATA_CAP 4096u
struct hac_page {uint64_t pa,word[512];unsigned level;};
struct hac_result {
 unsigned status,pages,reads[2],bytes[2],target_leaves;
 uint64_t begin,end;
 uint8_t data[2][HAC_DATA_CAP];
 struct hac_page table[HAC_TABLE_CAP];
};
struct hac_io {
 void *ctx;
 bool (*read)(void *,uint64_t,unsigned,uint64_t *);
 uint64_t (*now)(void *);
};
/* A complete RAM-only single-root graph, two exact passes; no target reads.
 * status1 complete,2 bounds/time,3 source,4 malformed graph,5 changed data. */
bool hac_capture(struct hac_result *,const struct hac_io *,uint64_t root);
