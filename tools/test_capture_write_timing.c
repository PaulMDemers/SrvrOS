#include <assert.h>
#include <stdio.h>
#include "../kernel/src/capture_write_timing_state.h"
int main(void) {
    struct capture_write_snapshot s={0};assert(!capture_write_touch(&s,1,10));
    s.enabled=1;assert(!capture_write_touch(&s,0,10));
    struct capture_write_record *r=capture_write_touch(&s,42,10);assert(r && r->pid==42);
    assert(capture_write_touch(&s,42,11)==r);
    r->attempts=2;
    capture_write_sample(r,CAP_WRITE_GATE,12,22);
    capture_write_sample(r,CAP_WRITE_RESERVE,23,26);
    capture_write_sample(r,CAP_WRITE_COPY,27,29);
    capture_write_sample(r,CAP_WRITE_HANDLER,22,30);
    capture_write_sample(r,CAP_WRITE_HANDOFF,30,44);
    capture_write_result(r,11,45,65536,65536);
    capture_write_result(r,46,49,123,-1);
    assert(r->calls==2 && r->bytes==65536 && r->requested==65659 && r->failures==1);
    assert(r->site[CAP_WRITE_CALL].count==2 && r->site[CAP_WRITE_CALL].total_us==37 && r->site[CAP_WRITE_CALL].max_us==34);
    assert(r->site[CAP_WRITE_GATE].total_us==10 && r->site[CAP_WRITE_HANDLER].total_us==8 && r->site[CAP_WRITE_HANDOFF].total_us==14);
    assert(r->site[CAP_WRITE_RESERVE].total_us==3 && r->site[CAP_WRITE_COPY].total_us==2 && !r->invalid);
    struct capture_write_snapshot copy=s;r->retired=1;assert(copy.records[0].retired==0);
    capture_write_sample(r,CAP_WRITE_COPY,50,51);assert(r->invalid);
    for(unsigned i=1;i<CAPTURE_WRITE_RECORDS;i++)assert(capture_write_touch(&s,100+i,60));
    assert(!capture_write_touch(&s,999,61) && s.dropped==1 && s.records[0].pid==42);
    struct capture_write_record bad={0};capture_write_sample(&bad,0,5,4);assert(bad.invalid);
    bad=(struct capture_write_record){0};capture_write_result(&bad,1,2,65537,1);assert(bad.invalid);
    bad=(struct capture_write_record){0};capture_write_result(&bad,1,2,1,2);assert(bad.invalid);
    bad=(struct capture_write_record){0};bad.site[0].count=UINT64_MAX;capture_write_sample(&bad,0,1,2);assert(bad.invalid && bad.site[0].count==UINT64_MAX);
    struct capture_write_record fast={0};
    capture_write_fast_result(&fast,65536,65536);
    capture_write_fast_result(&fast,5464,5464);
    assert(fast.fast_calls==2 && fast.fast_requested==71000 && fast.fast_bytes==71000 &&
        fast.fast_max_request==65536 && fast.fast_max_bytes==65536 && !fast.fast_failures && !fast.invalid);
    assert(!fast.gate.total.calls && !fast.site[CAP_WRITE_GATE].count); /* No fake gates. */
    capture_write_fast_result(&fast,17,-1);assert(fast.fast_failures==1 && fast.fast_bytes==71000);
    struct capture_write_record saved=fast;fast.retired=1;
    capture_write_fast_result(&fast,1,1);assert(fast.invalid && !saved.invalid && saved.fast_calls==3);
    bad=(struct capture_write_record){0};capture_write_fast_result(&bad,65537,1);assert(bad.invalid && !bad.fast_calls);
    bad=(struct capture_write_record){0};capture_write_fast_result(&bad,0,0);assert(bad.invalid);
    bad=(struct capture_write_record){0};capture_write_fast_result(&bad,1,2);assert(bad.invalid);
    bad=(struct capture_write_record){0};capture_write_fast_result(&bad,1,0);assert(bad.invalid);
    bad=(struct capture_write_record){.fast_requested=UINT64_MAX};capture_write_fast_result(&bad,1,1);assert(bad.invalid);
    puts("capture-write-timing: PASS disabled/identity slots, nested wall phases, counts/bytes/failures, copied snapshots, retirement, retained capacity refusal, clock/size/overflow evidence");
}
