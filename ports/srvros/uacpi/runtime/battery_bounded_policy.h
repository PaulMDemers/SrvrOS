#ifndef SRVROS_BATTERY_BOUNDED_POLICY_H
#define SRVROS_BATTERY_BOUNDED_POLICY_H
#include <stdbool.h>
#include <stdint.h>
/* Admission for first native five-sample trial; no service or mounted entry. */
static inline bool battery_bounded_entry(bool transitions,bool service,bool diagnostic,bool automatic,
    bool parked,bool mounted,bool image,bool io) {
    return !transitions&&!service&&diagnostic&&automatic&&parked&&!mounted&&!image&&!io;
}
/* Separately audited short host-event capture; never mounted/image/Home.
 * Coherent selects the service AC-before/after sampling path, not guided AC tests. */
static inline bool battery_bounded_capture_entry(bool coherent,bool service,bool owner,
    bool capture,bool automatic,bool parked,bool mounted,bool image,bool io) {
    return coherent&&service&&owner&&capture&&automatic&&parked&&!mounted&&!image&&!io;
}
/* AML only: SBS bank transactions use the separately serialized sampler. */
static inline bool battery_bounded_ec(bool write,uint64_t address,uint64_t value) {
    return write?(address==0x10&&!value):address==0x60;
}
#endif
