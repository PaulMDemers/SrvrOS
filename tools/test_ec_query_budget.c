#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../ports/srvros/uacpi/runtime/ec_query_budget.h"

static void retained(struct ec_query_budget *b, enum ec_query_fault fault) {
    assert(b->active && b->fault == fault);
    struct ec_query_budget saved = *b;
    assert(!ec_query_budget_begin(b, 0, 128, 2000000));
    assert(!ec_query_budget_take(b, b->last_us));
    assert(!ec_query_budget_finish(b, b->last_us));
    assert(!ec_query_budget_fail(b, EC_QUERY_AML));
    assert(!memcmp(b, &saved, sizeof(saved)));
}
int main(void) {
    struct ec_query_budget b = {0};
    assert(!ec_query_budget_begin(NULL, 0, 1, 1));
    assert(!ec_query_budget_check(NULL, 0));
    assert(!ec_query_budget_take(NULL, 0));
    assert(!ec_query_budget_finish(NULL, 0));
    assert(!ec_query_budget_fail(NULL, EC_QUERY_CONTEXT));
    assert(!ec_query_budget_take(&b, 0));
    assert(!ec_query_budget_finish(&b, 0));
    assert(!ec_query_budget_begin(&b, 0, 0, 1));
    assert(!ec_query_budget_begin(&b, 0, 1025, 1));
    assert(!ec_query_budget_begin(&b, 0, 1, 0));
    assert(!ec_query_budget_begin(&b, 0, 1, 5000001));
    assert(!b.active && !b.fault);
    /* Quiet drains can be reused. Observations and worker yields must not
     * replenish the attempt count; a notification cannot restart active work. */
    for (unsigned life = 0; life < 3; life++) {
        assert(ec_query_budget_begin(&b, 100, 128, 2000000));
        for (unsigned i = 0; i < 128; i++) {
            assert(ec_query_budget_check(&b, 100 + i));
            assert(!ec_query_budget_begin(&b, 100 + i, 1024, 5000000));
            assert(ec_query_budget_take(&b, 100 + i));
            assert(b.attempts == i + 1 && b.started_us == 100);
        }
        assert(ec_query_budget_finish(&b, 228));
        assert(!b.active && !b.fault && b.attempts == 128);
    }
    assert(ec_query_budget_begin(&b, 0, 128, 2000000));
    for (unsigned batch = 0; batch < 16; batch++)
        for (unsigned i = 0; i < 8; i++) assert(ec_query_budget_take(&b, batch * 8 + i));
    assert(!ec_query_budget_take(&b, 128));
    retained(&b, EC_QUERY_LIMIT);
    assert(b.attempts == 128);
    /* Queued time, inter-batch time and time inside a single AML handler count.
     * A quiet source at the end cannot erase an already missed deadline. */
    for (unsigned phase = 0; phase < 3; phase++) {
        b = (struct ec_query_budget){0};
        assert(ec_query_budget_begin(&b, 100, 4, 10));
        if (phase) assert(ec_query_budget_take(&b, 101));
        assert(ec_query_budget_check(&b, 109));
        assert(!(phase == 2 ? ec_query_budget_finish(&b, 110) : ec_query_budget_take(&b, 110)));
        retained(&b, EC_QUERY_DEADLINE);
    }
    b = (struct ec_query_budget){0};
    assert(ec_query_budget_begin(&b, 100, 2, 10));
    assert(ec_query_budget_check(&b, 105));
    assert(!ec_query_budget_check(&b, 104));
    retained(&b, EC_QUERY_CLOCK);
    b = (struct ec_query_budget){0};
    assert(ec_query_budget_begin(&b, UINT64_MAX - 5, 2, 10));
    assert(ec_query_budget_take(&b, UINT64_MAX));
    assert(!ec_query_budget_check(&b, 0));
    retained(&b, EC_QUERY_CLOCK);
    b = (struct ec_query_budget){0};
    assert(ec_query_budget_begin(&b, UINT64_MAX - 5, 2, 5));
    assert(!ec_query_budget_finish(&b, UINT64_MAX));
    retained(&b, EC_QUERY_DEADLINE);
    for (unsigned fault = EC_QUERY_CONTEXT; fault <= EC_QUERY_SCHEDULE; fault++) {
        b = (struct ec_query_budget){0};
        assert(ec_query_budget_begin(&b, 0, 1024, 5000000));
        assert(!ec_query_budget_fail(&b, EC_QUERY_OK) && !b.fault);
        assert(!ec_query_budget_fail(&b, 99) && !b.fault);
        assert(!ec_query_budget_fail(&b, fault));
        retained(&b, fault);
        assert(strcmp(ec_query_fault_name(fault), "invalid"));
    }
    assert(!strcmp(ec_query_fault_name(99), "invalid"));
    puts("ec-query-budget: PASS finite drains, repeated lifetimes, cross-batch limits, queued/AML deadlines, monotonic clock and sticky failure retention");
}
