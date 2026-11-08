#pragma once
#include <srvros/air61_read_window.h>
#include <srvros/ecam_boot.h>
#define AIR61_READ_RECEIPT_CAP 45000u
#define AIR61_READ_RAW_CAP 70000u
/* Separate isolated bridges. No native reset, returned target pointer or
 * device ownership grant. Read-enabled builds expose only the sample entry. */
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
bool haswell_air61_register_sample(void);
bool air61_sample_cache0(struct air61_sample_admission *);
bool air61_sample_cache1(struct air61_sample_admission *);
#else
bool haswell_air61_read_qualify(void);
#endif
const struct ecam_boot_gate_result *air61_read_gate_result0(void);
const struct ecam_boot_gate_result *air61_read_gate_result1(void);
const struct ecam_boot_gate_diagnostic *air61_read_gate_diagnostic0(void);
const struct ecam_boot_gate_diagnostic *air61_read_gate_diagnostic1(void);
bool air61_read_gate_check0(unsigned);
bool air61_read_gate_check1(unsigned);
bool air61_read_gate_ram0(uint64_t);
bool air61_read_gate_ram1(uint64_t);
uint64_t air61_read_gate_ram_steps0(void);
uint64_t air61_read_gate_ram_steps1(void);
bool air61_read_gates_equal(void);
/* RAM-only one-shot serialization, after clean complete retirement. The wire
 * cap leaves room for base64 framing in the unchanged 65536-byte saved tail.
 * On failure written=0 and no output is a valid receipt. No pointer/padding or
 * hashes replace raw observations. No hardware access or clock sampling. */
bool haswell_air61_read_encode(const char *,uint8_t *,unsigned,unsigned *);

bool air61_read_gate_touch1(void);
bool haswell_air61_read_report(const char *);
void haswell_air61_read_reference(const char *,bool);
