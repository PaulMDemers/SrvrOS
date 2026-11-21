#ifndef SRVROS_INTEL_HANDOFF_H
#define SRVROS_INTEL_HANDOFF_H
#include <srvros/scanout_handoff.h>
bool intel_window_handoff_snapshot(struct scanout_handoff_snapshot *);
bool intel_window_handoff_revalidate(const struct scanout_handoff_snapshot *);
bool intel_window_handoff_clean(void);
bool intel_handoff_probe_report(void);
#endif
