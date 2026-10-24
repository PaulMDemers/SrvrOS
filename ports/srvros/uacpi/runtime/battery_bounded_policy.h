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
/* Exclusive image file capture: storage I/O may overlap sampling only while
 * the exact parked image lease remains ready. Entry separately requires idle I/O. */
static inline bool battery_bounded_files_entry(bool coherent,bool service,bool owner,
    bool capture,bool automatic,bool parked,bool mounted,bool ready) {
    return coherent&&service&&owner&&capture&&automatic&&parked&&!mounted&&ready;
}
/* Explicit short Home diagnostic only; mounted failures must never reset. */
static inline bool battery_bounded_home_entry(bool coherent,bool service,bool owner,
    bool diagnostic,bool held,bool automatic,bool image) {
    return coherent&&service&&owner&&diagnostic&&held&&!automatic&&!image;
}
/* Capability belongs only to the generated, non-continuous Home session. */
static inline bool battery_bounded_home_session(bool diagnostic,bool controls,
    bool continuous,bool desktop,bool capture,bool session) {
    return diagnostic&&controls&&!continuous&&!desktop&&!capture&&!session;
}
/* Timed boot capture only; never ordinary sessions or attended transitions. */
static inline bool battery_bounded_home_desktop(bool diagnostic,bool controls,
    bool continuous,bool desktop,bool capture,bool session,bool transitions) {
    return diagnostic&&controls&&continuous&&desktop&&capture&&!session&&!transitions;
}
/* Separate attended capture capability; never grants ordinary session entry. */
static inline bool battery_bounded_home_transitions(bool diagnostic,bool controls,
    bool continuous,bool desktop,bool capture,bool session,bool transitions) {
    return diagnostic&&controls&&continuous&&desktop&&capture&&!session&&transitions;
}
/* AML only: SBS bank transactions use the separately serialized sampler. */
static inline bool battery_bounded_ec(bool write,uint64_t address,uint64_t value) {
    return write?(address==0x10&&!value):address==0x60;
}
#endif
