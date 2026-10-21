#ifndef SRVROS_EC_QUERY_REARM_H
#define SRVROS_EC_QUERY_REARM_H
#include <stdbool.h>
/* The caller owns the suspended GPE and serializes this sequence against IRQ
 * dispatch. Callbacks retain transport/error/queue ownership. Never enable a
 * source already known pending: that can latch an IRQ before queue() masks and
 * clears the event. Keep the post-enable check for arrivals during this window.
 * A late event may still latch an IRQ; this helper does not change IRQ claims. */
struct ec_query_rearm_ops {
    void *context;
    bool (*pending)(void *);
    void (*resume)(void *);
    void (*queue)(void *);
};
static inline void ec_query_rearm(const struct ec_query_rearm_ops *ops) {
    if (ops->pending(ops->context)) {
        ops->queue(ops->context);
        return;
    }
    ops->resume(ops->context);
    if (ops->pending(ops->context)) ops->queue(ops->context);
}
#endif
