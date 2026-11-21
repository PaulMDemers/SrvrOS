#ifndef SRVROS_INTEL_READER_AUDIT_H
#define SRVROS_INTEL_READER_AUDIT_H
#include <stdbool.h>
#include <stdint.h>
#define INTEL_READER_WORDS 13u
/* Broadwell display pages already covered by the native reader UC contract.
 * Non-posted TXP is not a posted-write/prefetch/firmware drain primitive. */
extern const uint32_t intel_reader_offsets[INTEL_READER_WORDS];
struct intel_reader_sample { uint32_t config[6],plane[3][INTEL_READER_WORDS]; };
struct intel_reader_result {
 const struct intel_reader_result *object;
 struct intel_reader_sample sample[2];
 uint64_t begin_us,end_us,last_us;
 unsigned samples,configs,planes,reads,reason,primary,cursor,sprite,mismatch;
 bool stable,nonposted_clear,complete;
};
struct intel_reader_io {
 void *ctx;
 bool (*guard)(void *); /* Live pinned HDA owner, command2, no ambiguous maps. */
 bool (*config)(void *,unsigned,uint32_t *);
 bool (*plane)(void *,unsigned,uint32_t [INTEL_READER_WORDS]);
 uint64_t (*now)(void *);
};
/* Zero-init, one-use. Two passes are separated by the caller's full HDA check.
 * No writes, wait-to-drain, ownership grant or inherited-allocation release. */
bool intel_reader_pass(struct intel_reader_result *,struct intel_reader_io,unsigned);
bool intel_reader_finish(struct intel_reader_result *,struct intel_reader_io);
#endif
