#include <srvros/intel_takeover_preflight.h>
static const uint32_t takeover_slots[INTEL_TAKEOVER_WORDS]={3,4,5,6,7,9};
static const uint32_t takeover_registers[INTEL_TAKEOVER_WORDS]={0x20d0,0x120d0,0x1a0d0,0x1c0d0,0x220d0,0x941c};
static bool takeover_sample(void *ctx,uint32_t out[INTEL_TAKEOVER_WORDS],unsigned *count) {
    if(!out || !count || !engine_guard(ctx))return false;
    *count=0;
    for(unsigned i=0;i<INTEL_TAKEOVER_WORDS;i++) {
        unsigned slot=takeover_slots[i];
        if(!completion_mapping(ctx,slot) || !engine_requesters(ctx))return false;
        uint32_t reg=takeover_registers[i];
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
        if(!intel_display_test_takeover_read(i,reg,&out[i]))return false;
#else
        out[i]=*(volatile const uint32_t *)(WAKE_VA+slot*4096+(reg&4095));
#endif
        ++*count;
        if(out[i]==UINT32_MAX)return false;
    }
    return engine_requesters(ctx);
}
static void takeover_report(const struct intel_takeover_result *r,const struct intel_completion_result *c) {
    for(unsigned n=0;n<r->attempts && n<2;n++) {
        console_printf("intel-takeover-row: v=1 s=%u n=%u d=",(uint64_t)n,(uint64_t)r->counts[n]);
        if(!r->counts[n])console_write("-");
        for(unsigned i=0;i<r->counts[n] && i<INTEL_TAKEOVER_WORDS;i++)console_printf("%s%u",i?",":"",(uint64_t)r->words[n][i]);
        console_write("\n");
    }
    console_printf("intel-takeover-summary: "
#ifdef SRVROS_INTEL_RING_RESET
        "v=2 "
#else
        "v=1 "
#endif
        "attempts=%u samples=%u reads=%u begin_us=%u end_us=%u epoch=%u batches=%u blocked=%u borrowed=%u returned=%u retained=%u stable=%u busy=%u reserved=%u geometry=%u quiescent=%u reason=%u complete=%u obligations=255 reset_writes=0 pte_writes=0 managed_pages=0 hardware_ownership=0"
#ifdef SRVROS_INTEL_RING_RESET
        " last_us=%u"
#endif
        "\n",
        (uint64_t)r->attempts,(uint64_t)r->samples,(uint64_t)r->reads,r->begin_us,r->end_us,(uint64_t)r->epoch,(uint64_t)c->batches,(uint64_t)r->blocked,(uint64_t)(r->borrow.parent!=NULL),(uint64_t)r->returned,(uint64_t)r->borrow.active,(uint64_t)r->stable,(uint64_t)r->busy,(uint64_t)r->reserved,(uint64_t)r->geometry,(uint64_t)r->quiescent,(uint64_t)r->reason,(uint64_t)r->complete
#ifdef SRVROS_INTEL_RING_RESET
        ,r->last_us
#endif
    );
}
