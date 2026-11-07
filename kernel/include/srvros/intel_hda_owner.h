#ifndef SRVROS_INTEL_HDA_OWNER_H
#define SRVROS_INTEL_HDA_OWNER_H
#include <srvros/intel_display_hda.h>
/* Cooperative EFI submission policy, not SMM exclusion or DMA completion.
 * Only an isolated boot adapter may supply these same-boot facts. */
struct intel_hda_owner_facts {
 bool compiled,recipe,efi64,loader,profile,serialized,excluded,prerequisites;
};
struct intel_hda_owner_result {
 const struct intel_hda_owner_result *object;
 struct intel_hda_result phase[3]; /* initial, inhibited, restored */
 uint64_t begin_us,end_us,last_us;
 unsigned phases,attempts,writes,reason;
 bool admitted,inhibited,restored,retained,complete;
};
struct intel_hda_owner_io {
 void *ctx;
 bool (*inspect)(void *,struct intel_hda_result *);
 bool (*guard)(void *); /* Exact resource/cache/CPU exclusion, no mutations. */
 bool (*endpoint)(void *,struct intel_hda_endpoint *);
 /* Only PCI command16 at offset4. False may mean a store occurred. */
 bool (*command16)(void *,uint16_t);
 uint64_t (*now)(void *);
};
bool intel_hda_owner_idle(const struct intel_hda_result *,unsigned command);
bool intel_hda_owner_trial(struct intel_hda_owner_result *,struct intel_hda_owner_facts,struct intel_hda_owner_io);
bool intel_display_hda_owner_report(void);
bool intel_display_hda_owner_retained(void);
#endif
