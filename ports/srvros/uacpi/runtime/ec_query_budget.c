#include "ec_query_budget.h"

bool ec_query_budget_fail(struct ec_query_budget *b, enum ec_query_fault fault) {
    if (b && b->active && b->fault == EC_QUERY_OK &&
        fault > EC_QUERY_OK && fault <= EC_QUERY_SCHEDULE) b->fault = fault;
    return false;
}
bool ec_query_budget_begin(struct ec_query_budget *b, uint64_t now,
    unsigned max_attempts, uint64_t timeout_us) {
    if (!b || b->active || b->fault || !max_attempts || max_attempts > 1024 ||
        !timeout_us || timeout_us > 5000000) return false;
    *b = (struct ec_query_budget){.started_us=now, .last_us=now,
        .timeout_us=timeout_us, .limit=max_attempts, .active=true};
    return true;
}
bool ec_query_budget_check(struct ec_query_budget *b, uint64_t now) {
    if (!b || !b->active || b->fault) return false;
    if (now < b->last_us) return ec_query_budget_fail(b, EC_QUERY_CLOCK);
    b->last_us = now;
    /* Subtraction also handles a start near UINT64_MAX without a wrapped end. */
    if (now - b->started_us >= b->timeout_us)
        return ec_query_budget_fail(b, EC_QUERY_DEADLINE);
    return true;
}
bool ec_query_budget_take(struct ec_query_budget *b, uint64_t now) {
    if (!ec_query_budget_check(b, now)) return false;
    if (b->attempts >= b->limit) return ec_query_budget_fail(b, EC_QUERY_LIMIT);
    b->attempts++;
    return true;
}
bool ec_query_budget_finish(struct ec_query_budget *b, uint64_t now) {
    if (!ec_query_budget_check(b, now)) return false;
    b->active = false;
    return true;
}
const char *ec_query_fault_name(enum ec_query_fault fault) {
    switch (fault) {
    case EC_QUERY_OK: return "none";
    case EC_QUERY_CLOCK: return "clock";
    case EC_QUERY_DEADLINE: return "deadline";
    case EC_QUERY_LIMIT: return "limit";
    case EC_QUERY_CONTEXT: return "context";
    case EC_QUERY_TRANSPORT: return "transport";
    case EC_QUERY_AML: return "aml";
    case EC_QUERY_SCHEDULE: return "schedule";
    default: return "invalid";
    }
}
