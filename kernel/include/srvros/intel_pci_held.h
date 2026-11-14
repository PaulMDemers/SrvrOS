#ifndef SRVROS_INTEL_PCI_HELD_H
#define SRVROS_INTEL_PCI_HELD_H
#include <srvros/intel_engine_prepare.h>
#include <srvros/intel_pci_requester.h>
enum intel_pci_held_reason {IPH_INPUT=1,IPH_CLOCK=2,IPH_PARENT=4,
 IPH_GUARD=8,IPH_READ=16,IPH_PROFILE=32};
struct intel_pci_held_result {
 const struct intel_pci_held_result *object;
 const struct intel_engine_prepare_result *preparation;
 const struct intel_gt_access *parent;
 const struct intel_gt_borrow *borrow;
 uint32_t epoch,words[2][INTEL_PCI_REQUESTER_WORDS];
 uint64_t origin_us,begin_us,last_us,end_us,clock_at,clock_prev;
 unsigned attempts,reads,samples,profiles,counts[2],reason,bus_master,pending,flr;
 bool stable,complete;
};
struct intel_pci_held_io {
 void *ctx;
 bool (*guard)(void *);
 /* RO ordered DWORD load at the exact supplied PCI offset. True means one
  * complete word; false retains its attempt but supplies no complete word.
  * Provider exit failure after true is independently caught by the guard. */
 bool (*read)(void *,uint32_t,uint32_t *);
 /* Same monotonic timebase as preparation; never a restarted action clock. */
 uint64_t (*now)(void *);
};
/* Original zero-init one-use observation ONLY during the original prepare
 * action callback. Uses the EXISTING registered dependency without creating,
 * returning or changing it. Full guards bracket each individual read, with
 * fixed-offset prefix gating before later capability/payload reads. The shared
 * prepare100ms budget is never restarted. A completed capture, TP-clear sample
 * or stable status is NOT DMA drain, requester exclusion, cache/firmware/reader
 * retirement or permission for reset/PTE writes, mapping or memory release.
 * Failure returns false so the enclosing preparation retains uncertainty. */
bool intel_pci_held_observe(struct intel_pci_held_result *,
 const struct intel_engine_prepare_result *,struct intel_pci_held_io);
#endif
