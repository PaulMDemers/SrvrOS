#include "sci_owner.h"
#define MASKED (1u << 16)
#define PENDING ((1u << 12) | (1u << 14))
static bool line_valid(unsigned line) {
    return line < 16 && line != 0 && line != 1 && line != 2 && line != 4 && line != 12;
}
bool sci_route_resolve(struct sci_route *out, uint16_t sci,
    const struct acpi_interrupt_source_override *override) {
    if (!out) return false;
    *out = (struct sci_route){0};
    if (!line_valid(sci)) return false;
    struct sci_route route = {.gsi = sci, .line = sci, .active_low = true};
    if (override && override->present) {
        unsigned polarity = override->flags & 3, trigger = (override->flags >> 2) & 3;
        if (override->bus || override->source != sci || (override->flags & ~15u) ||
            polarity == 2 || trigger == 1 || trigger == 2) return false;
        route.gsi = override->gsi;
        route.active_low = polarity != 1;
    }
    *out = route;
    return true;
}
static bool poison(struct sci_owner *o) { o->phase = SCI_OWNER_POISONED; return false; }
static bool rollback(struct sci_owner *o) {
    /* prepare can fail after a partial hardware write. Confirm masking before
     * restoring, even though the saved route was masked. */
    if (!o->ops.mask(o->ops.context, o->route.gsi, true) ||
        !o->ops.restore(o->ops.context, o->route.gsi, o->saved_low, o->saved_high))
        return poison(o);
    *o = (struct sci_owner){0};
    return false;
}
bool sci_owner_prepare(struct sci_owner *o, struct sci_owner_ops ops,
    struct sci_route route, irq_handler_fn handler, void *context) {
    if (!o || o->phase != SCI_OWNER_OFF || o->token || !line_valid(route.line) || !handler ||
        !ops.read || !ops.prepare || !ops.mask || !ops.restore ||
        !ops.register_irq || !ops.remove_irq) return false;
    uint32_t low, high;
    if (!ops.read(ops.context, route.gsi, &low, &high) || !(low & MASKED) ||
        (low & PENDING) || (high & 0x00ffffff)) return false;
    *o = (struct sci_owner){.ops = ops, .route = route,
        .saved_low = low, .saved_high = high, .phase = SCI_OWNER_MASKED};
    if (!ops.prepare(ops.context, route.gsi, 32 + route.line, route.active_low, true))
        return rollback(o);
    o->token = ops.register_irq(ops.context, route.line, handler, context);
    if (!o->token) return rollback(o);
    return true;
}
bool sci_owner_enable(struct sci_owner *o) {
    if (!o || o->phase != SCI_OWNER_MASKED || !o->token) return false;
    /* Callback/context already exist when an unmask delivers immediately. */
    o->phase = SCI_OWNER_ACTIVE;
    if (o->ops.mask(o->ops.context, o->route.gsi, false)) return true;
    if (!o->ops.mask(o->ops.context, o->route.gsi, true)) return poison(o);
    o->phase = SCI_OWNER_MASKED;
    return false;
}
bool sci_owner_mask(struct sci_owner *o) {
    if (!o || (o->phase != SCI_OWNER_ACTIVE && o->phase != SCI_OWNER_MASKED) || !o->token)
        return false;
    if (!o->ops.mask(o->ops.context, o->route.gsi, true)) return poison(o);
    o->phase = SCI_OWNER_MASKED;
    return true;
}
bool sci_owner_release(struct sci_owner *o) {
    if (!o) return false;
    if (o->phase == SCI_OWNER_OFF) return !o->token;
    if (!sci_owner_mask(o)) return false;
    if (!o->ops.remove_irq(o->ops.context, o->token)) return poison(o);
    o->token = 0;
    if (!o->ops.restore(o->ops.context, o->route.gsi, o->saved_low, o->saved_high))
        return poison(o);
    *o = (struct sci_owner){0};
    return true;
}
