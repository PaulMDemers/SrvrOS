#ifndef SRVROS_INTEL_PCI_REQUESTER_H
#define SRVROS_INTEL_PCI_REQUESTER_H
#include <srvros/intel_takeover_preflight.h>
#define INTEL_PCI_REQUESTER_WORDS 9u
#define INTEL_PCI_REQUESTER_TOTAL_US 100000ull
extern const uint32_t intel_pci_requester_offsets[INTEL_PCI_REQUESTER_WORDS];
enum intel_pci_requester_reason {IPR_INPUT=1,IPR_CLOCK=2,IPR_PARENT=4,
 IPR_GUARD=8,IPR_READ=16,IPR_COUNT=32,IPR_PROFILE=64,IPR_RETURN=128};
struct intel_pci_requester_result {
 const struct intel_pci_requester_result *object;
 struct intel_gt_access *parent;
 const struct intel_takeover_result *preflight;
 struct intel_gt_borrow borrow;
 uint32_t epoch,words[2][INTEL_PCI_REQUESTER_WORDS];
 uint64_t begin_us,last_us,end_us,clock_at,clock_prev;
 unsigned attempts,samples,profiles,reads,counts[2],reason,bus_master,pending,flr;
 bool blocked,stable,returned,complete;
};
struct intel_pci_requester_io {
 void *ctx;
 bool (*guard)(void *);
 bool (*sample)(void *,uint32_t [INTEL_PCI_REQUESTER_WORDS],unsigned *);
 uint64_t (*now)(void *);
};
/* RO two-sample PCI AF observation after an original returned preflight.
 * No command/BME/FLR/reset/PTE writer. Busy/pending bits are observations, not
 * errors or comprehensive DMA-drain witnesses. Original zero-init one-use
 * result/registered borrow only; unknown parent state remains pinned. */
/* Gate each next fixed PCI read on the already captured exact prefix. */
bool intel_pci_requester_prefix(const uint32_t *,unsigned);
bool intel_pci_requester_observe(struct intel_pci_requester_result *,
 struct intel_gt_access *,const struct intel_takeover_result *,struct intel_pci_requester_io);
#endif
