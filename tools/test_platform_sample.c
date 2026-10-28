#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint64_t test_ticks=100;
static unsigned reads;
static uint32_t rc=50;
static uint64_t platform_test_tsc(void) {return ++test_ticks;}
static uint32_t platform_test_read(unsigned off) {
    assert(off==0x60 || off==0x108);reads++;
    return off==0x60?3:rc++;
}
#define SRVROS_PLATFORM_HOST_TEST
#include "../kernel/diagnostics/platform_audit.c"
uint64_t timer_monotonic_us(void) {return ++test_ticks;}
int main(void) {
    platform_audit_sample(1);assert(!reads && !series_count);
    mapped=true;platform_audit_sample(1);assert(!reads && !series_count);
    series_active=true;mapped=false;platform_audit_sample(1);assert(!reads);
    mapped=true;
    for(unsigned i=0;i<24;i++)platform_audit_sample(1);
    assert(series_count==12 && reads==48 && !fault && !series_overflow);
    for(unsigned i=0;i<12;i++) {
        const struct series_row *r=&series[i];
        assert(r->pm0<r->tsc0 && r->tsc0<r->tsc1 && r->tsc1<r->pm1);
        assert(r->r1==r->r0+1 && r->s0==3 && r->s1==3);
    }
    platform_audit_sample(1);assert(series_count==12 && reads==48 && fault && series_overflow);
    series_active=false;platform_audit_sample(1);assert(reads==48);
    series_active=true;series_count=0;series_checkpoints=0;fault=0;rc=0xfffffffeu;
    platform_audit_sample(1);assert(fault && series[0].r1==0xffffffffu);
    puts("platform-sample: PASS inactive/unbound refusal, ordered reads, capacity/overflow, terminal inactivity, all-ones failure");
}
