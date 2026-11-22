#include <srvros/x86_cache.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct x86_cache_snapshot raw,snapshot;
static unsigned calls,fail_at,change_at,cpu_calls,change_cpu;
static unsigned clock_calls,jump_at;
static uint64_t time_us;static bool freeze,backward;
static bool cpu(void *ctx,struct x86_cache_cpu *c) {
    assert(ctx==&raw);*c=raw.cpu;if(++cpu_calls==change_cpu)c->cr0^=1;return true;
}
static bool msr(void *ctx,uint32_t n,uint64_t *v) {
    assert(ctx==&raw);calls++;
    if(calls==fail_at)return false;
    if(n==0xfe)*v=raw.cap;
    else if(n==0x277)*v=raw.pat;
    else if(n==0x2ff)*v=raw.def_type;
    else if(n>=0x200 && n<0x200+2*(raw.cap&255)) {
        unsigned i=(n-0x200)/2;assert(i<16);*v=(n&1)?raw.mask[i]:raw.base[i];
    } else {
        const unsigned fixed[]={0x250,0x258,0x259,0x268,0x269,0x26a,0x26b,0x26c,0x26d,0x26e,0x26f};
        bool found=false;for(unsigned i=0;i<11;i++)if(n==fixed[i]){*v=raw.fixed[i];found=true;break;}
        assert(found && (raw.cap&256));
    }
    if(calls==change_at)*v^=1;
    return true;
}
static uint64_t now(void *ctx) {
    assert(ctx==&raw);
    if(++clock_calls==jump_at)time_us+=100001;
    if(backward)return time_us--;
    if(!freeze)time_us++;
    return time_us;
}
static const struct x86_cache_io io={&raw,cpu,msr,now};
static void setup(void) {
    raw=(struct x86_cache_snapshot){.cpu={{0x756e6547,0x49656e69,0x6c65746e},0x306d4,0x11000,39,0x80050033},
        .pat=0x0007040600070406ull,.cap=0x508,.def_type=0xc06};
    for(unsigned i=0;i<11;i++)raw.fixed[i]=0x0606060606060606ull;
    calls=fail_at=change_at=cpu_calls=change_cpu=0;time_us=100;clock_calls=jump_at=0;freeze=backward=false;
}
static void capture(void){assert(x86_cache_snapshot(&snapshot,&io));assert(snapshot.status==CACHE_OBSERVED);}
static void expect(uint64_t p,unsigned index,unsigned m,unsigned pat,unsigned effective) {
    unsigned a=255,b=255,c=255;assert(x86_cache_page(&snapshot,p,index,&a,&b,&c));assert(a==m && b==pat && c==effective);
}
static void refuse(uint64_t p,unsigned index) {
    unsigned a=99,b=98,c=97;assert(!x86_cache_page(&snapshot,p,index,&a,&b,&c));assert(a==99 && b==98 && c==97);
}
static void range(unsigned i,uint64_t base,uint64_t size,unsigned type) {
    assert(i<16 && size>=4096 && !(size&(size-1)) && !(base&(size-1)));
    raw.base[i]=base|type;raw.mask[i]=(((1ull<<39)-1) & ~(size-1))|0x800;
}
static void span(uint64_t physical,uint32_t pages,unsigned index) {
    unsigned m=99,p=98,e=97,n=96,a=0,b=0,c=0;
    bool expected=pages && pages<=4096 && physical>=0x100000 && !(physical&4095) &&
        index<8 && physical<=(1ull<<snapshot.cpu.physical_bits)-(uint64_t)pages*4096;
    if(expected)expected=x86_cache_page(&snapshot,physical,index,&a,&b,&c);
    for(unsigned i=1;expected && i<pages;i++) {
        unsigned x,y,z;
        expected=x86_cache_page(&snapshot,physical+4096ull*i,index,&x,&y,&z) && x==a && y==b && z==c;
    }
    assert(x86_cache_range(&snapshot,physical,pages,index,&m,&p,&e,&n)==expected);
    if(expected)assert(m==a && p==b && e==c && n>=1 && n<=33);
    else assert(m==99 && p==98 && e==97 && n==96);
}
int main(void) {
    setup();capture();unsigned reads=calls;assert(reads==33 && snapshot.reads==reads);
    struct x86_cache_snapshot saved=snapshot;assert(x86_cache_equal(&saved,&snapshot));
    expect(0xc0000000,0,6,6,6);expect(0xc0000000,2,6,7,0);expect(0xc0000000,3,6,0,0);
    /* Exhaust all30 valid Intel table12-7 combinations, including UC- with WP
     * and WC, and PAT WC overriding MTRR UC. Not a simplistic min(type) rule. */
    const unsigned types[]={0,1,4,5,6},pats[]={0,1,4,5,6,7};
    const unsigned expected[5][6]={{0,1,0,0,0,0},{0,1,0,0,1,1},{0,1,4,5,4,0},{0,1,4,5,5,1},{0,1,4,5,6,0}};
    for(unsigned m=0;m<5;m++)for(unsigned p=0;p<6;p++) {
        setup();raw.def_type=0xc00|types[m];raw.pat=(raw.pat&~255ull)|pats[p];capture();
        expect(0x400000,0,types[m],pats[p],expected[m][p]);
    }
    setup();range(0,0xb0000000,0x10000000,1);capture();
    expect(0xb2200000,2,1,7,1);expect(0xb2200000,3,1,0,0); /* PCD alone can be WC. */
    setup();range(0,0x400000,0x400000,6);range(1,0x600000,0x200000,4);capture();
    expect(0x5ff000,0,6,6,6);expect(0x600000,0,4,6,4);expect(0x7ff000,0,4,6,4);expect(0x800000,0,6,6,6);
    range(2,0x600000,0x100000,0);capture();expect(0x600000,2,0,7,0);
    range(2,0x600000,0x100000,1);capture();refuse(0x600000,3); /* Undefined triple overlap. */
    setup();raw.def_type=6;raw.pat=0x0101010101010101ull;capture();expect(0xc0000000,0,0,1,0); /* E=0 forces UC. */
    setup();range(0,0,1ull<<39,6);capture();expect((1ull<<39)-4096,0,6,6,6);
    refuse(0,0);refuse(0xff000,0);refuse(0x100001,0);refuse(1ull<<39,0);refuse(0x100000,8);
    for(unsigned t=0;t<8;t++)if(t==2 || t==3) {setup();raw.pat=(raw.pat&~255ull)|t;assert(!x86_cache_snapshot(&snapshot,&io) && snapshot.status==CACHE_INVALID);}
    setup();raw.pat|=1ull<<63;assert(!x86_cache_snapshot(&snapshot,&io));
    setup();raw.def_type|=0x100;assert(!x86_cache_snapshot(&snapshot,&io));
    setup();range(0,0x400000,0x200000,6);raw.mask[0]^=1ull<<16;assert(!x86_cache_snapshot(&snapshot,&io));
    setup();range(0,0x400000,0x200000,6);raw.base[0]|=0x1000;assert(!x86_cache_snapshot(&snapshot,&io));
    setup();range(0,0x400000,0x200000,6);raw.base[0]|=1ull<<39;assert(!x86_cache_snapshot(&snapshot,&io));
    setup();range(0,0x400000,0x200000,6);raw.mask[0]|=1;assert(!x86_cache_snapshot(&snapshot,&io));
    setup();raw.cap=0x511;assert(!x86_cache_snapshot(&snapshot,&io) && calls==1);
    setup();raw.cpu.features&=~0x1000;assert(!x86_cache_snapshot(&snapshot,&io) && !calls);
    setup();raw.cpu.vendor[0]^=1;assert(!x86_cache_snapshot(&snapshot,&io) && !calls);
    setup();raw.cpu.cr0|=1ull<<30;assert(!x86_cache_snapshot(&snapshot,&io) && !calls);
    setup();raw.cpu.physical_bits=53;assert(!x86_cache_snapshot(&snapshot,&io) && !calls);
    setup();raw.cap=0;raw.def_type=0x806;capture();assert(calls==6 && !snapshot.fixed_count && !snapshot.count);
    setup();raw.cap=0x510;capture();assert(calls==49 && snapshot.count==16);
    for(unsigned i=1;i<=reads;i++){setup();fail_at=i;assert(!x86_cache_snapshot(&snapshot,&io) && snapshot.status==CACHE_READ && calls==i);}
    for(unsigned i=reads-2;i<=reads;i++){setup();change_at=i;assert(!x86_cache_snapshot(&snapshot,&io) && snapshot.status==CACHE_CHANGED);}
    for(unsigned i=2;i<=2*reads+2;i++){setup();jump_at=i;assert(!x86_cache_snapshot(&snapshot,&io) && snapshot.status==CACHE_CLOCK);}
    for(unsigned bits=36;bits<=52;bits+=16){setup();raw.cpu.physical_bits=bits;capture();expect((1ull<<bits)-4096,0,6,6,6);refuse(1ull<<bits,0);}
    setup();change_cpu=2;assert(!x86_cache_snapshot(&snapshot,&io) && snapshot.status==CACHE_CHANGED);
    setup();freeze=true;assert(!x86_cache_snapshot(&snapshot,&io) && snapshot.status==CACHE_CLOCK);
    setup();backward=true;assert(!x86_cache_snapshot(&snapshot,&io) && snapshot.status==CACHE_CLOCK);
    setup();capture();snapshot.base[7]=1;assert(!x86_cache_equal(&saved,&snapshot));
    setup();capture();span(0x1000000,1356,0);span(0x1000000,1356,2);
    span(0x1000000,0,0);span(0x1000000,4097,0);span(0x1000001,1,0);
    span(0xff000,2,0);span(UINT64_MAX-4095,2,0);span(0x1000000,1,8);
    span((1ull<<39)-4096,1,0);span((1ull<<39)-4096,2,0);
    /* Equal WB endpoints conceal a single interior UC page. Every page in
     * the span is the independent oracle for the bounded boundary algorithm. */
    for(unsigned i=1;i<63;i++) {
        setup();range(0,0x1000000+4096ull*i,4096,0);capture();
        span(0x1000000,64,0);span(0x1000000+4096ull*i,1,0);
    }
    /* Many overlapping boundaries, UC precedence and WT/WB overlap. */
    for(unsigned scenario=0;scenario<32;scenario++) {
        setup();
        for(unsigned i=0;i<8;i++)range(i,0x1000000+4096ull*((scenario*13+i*7)%64),4096,(i+scenario)%3==0?0:6);
        capture();for(unsigned index=0;index<4;index++)span(0x1000000,64,index);
    }
    setup();range(0,0x1000000,0x200000,6);range(1,0x1100000,0x100000,4);capture();span(0x1000000,512,0);span(0x1100000,256,0);
    setup();raw.def_type=6;capture();span(0x1000000,1356,0);
    setup();raw.cpu.physical_bits=52;capture();span((1ull<<52)-4096,1,0);
    puts("x86-cache: PASS architectural matrix, UC-/WC trap, overlap/boundary/width refusal, 33 read faults, stable endpoints and bounded no-write snapshots");
}
