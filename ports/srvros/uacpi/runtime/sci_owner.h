#ifndef SRVROS_ACPI_SCI_OWNER_H
#define SRVROS_ACPI_SCI_OWNER_H
#include <srvros/acpi.h>
#include <srvros/irq_handlers.h>
/* CPU0, one lifecycle caller, outside IRQ dispatch. This owns only the route
 * and callback. The caller owns firmware/source configuration and must quiesce
 * device sources before release, then drain work before freeing its context.
 * Backends must verify hardware writes. register_irq returning zero guarantees
 * no callback was installed; failed hardware writes may have partial effects. */
struct sci_route { uint32_t gsi; uint8_t line; bool active_low; };
struct sci_owner_ops {
    void *context;
    bool (*read)(void *, uint32_t, uint32_t *, uint32_t *);
    bool (*prepare)(void *, uint32_t, uint8_t, bool, bool);
    bool (*mask)(void *, uint32_t, bool);
    bool (*restore)(void *, uint32_t, uint32_t, uint32_t);
    uint64_t (*register_irq)(void *, uint8_t, irq_handler_fn, void *);
    bool (*remove_irq)(void *, uint64_t);
};
enum sci_owner_phase { SCI_OWNER_OFF, SCI_OWNER_MASKED, SCI_OWNER_ACTIVE, SCI_OWNER_POISONED };
struct sci_owner {
    struct sci_owner_ops ops;
    struct sci_route route;
    uint32_t saved_low, saved_high;
    uint64_t token;
    enum sci_owner_phase phase;
};
/* Supported ISA SCI subset; conforming flags mean active-low/level for SCI.
 * An explicit edge trigger, reserved flags or a reserved IRQ stub is refused.
 * The IO-APIC backend separately validates the resolved GSI's topology. */
bool sci_route_resolve(struct sci_route *, uint16_t sci,
    const struct acpi_interrupt_source_override *);
/* Start from a zeroed owner and a masked, nonpending route. Success retains a
 * registered callback with the route masked. False with OFF means rollback
 * succeeded; POISONED means retain all state and halt, never reset/reuse it. */
bool sci_owner_prepare(struct sci_owner *, struct sci_owner_ops,
    struct sci_route, irq_handler_fn, void *callback_context);
bool sci_owner_enable(struct sci_owner *);
bool sci_owner_mask(struct sci_owner *);
bool sci_owner_release(struct sci_owner *);
#endif
