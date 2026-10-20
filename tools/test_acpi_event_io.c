#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ports/srvros/uacpi/runtime/event_io.h"
static uint8_t fa[244];
static struct acpi_event_io p;
static void put32(unsigned off, unsigned value) {
    for (unsigned i=0;i<4;i++) fa[off+i]=value>>(8*i);
}
static void sum(void) {
    fa[9]=0; uint8_t s=0;
    for (unsigned i=0;i<sizeof(fa);i++) s+=fa[i];
    fa[9]=-s;
}
static void gas(unsigned off,unsigned address,unsigned bits,unsigned access) {
    fa[off]=1;fa[off+1]=bits;fa[off+3]=access;put32(off+4,address);
}
static void init(void) {
    memset(fa,0,sizeof(fa)); memcpy(fa,"FACP",4);put32(4,sizeof(fa));
    fa[46]=9;put32(48,0xb2);fa[52]=0xf0;fa[53]=0xf1;
    put32(56,0x1800);put32(64,0x1804);put32(72,0x1850);put32(76,0x1808);
    fa[88]=4;fa[89]=2;fa[90]=1;fa[91]=4;
    put32(80,0x1880);fa[92]=32;
    gas(148,0x1800,32,3);gas(172,0x1804,16,2);gas(196,0x1850,8,1);
    gas(208,0x1808,32,3);gas(220,0x1880,0,1);sum();
}
static bool parse(void) { sum();return acpi_event_resources_parse(&p.resources,fa,sizeof(fa))==ACPI_EC_RESOURCES_OK; }
static void rejected(void) {
    memset(&p.resources,0xa5,sizeof(p.resources));assert(!parse());
    assert(!p.resources.sci && !p.resources.pm1_status && !p.resources.gpe[0].bytes);
}
int main(int argc,char **argv) {
    init();assert(parse());
    assert(p.resources.pm1_status==0x1800 && p.resources.pm1_enable==0x1802 &&
        p.resources.pm1_control==0x1804 && p.resources.timer==0x1808 &&
        p.resources.pm2_control==0x1850 && p.resources.gpe[0].status==0x1880 &&
        p.resources.gpe[0].enable==0x1890 && p.resources.gpe[0].bytes==16);
    p.fixed_enable_mask=0x120;p.gpe_enable_mask[0][9]=0x40;
    for (unsigned bits=8;bits<=32;bits*=2) {
        assert(acpi_event_io_read(&p,0x1800,bits)==(bits==16));
        assert(acpi_event_io_read(&p,0x1808,bits)==(bits==32));
        assert(acpi_event_io_read(&p,0x1889,bits)==(bits==8));
        assert(!acpi_event_io_read(&p,0xb2,bits));
        assert(!acpi_event_io_write(&p,0x1804,bits,1));
        assert(!acpi_event_io_write(&p,0x1808,bits,0));
        assert(!acpi_event_io_write(&p,0x1850,bits,0));
    }
    for (unsigned v=0;v<65536;v++) {
        assert(acpi_event_io_write(&p,0x1800,16,v)); /* W1C, no ordinary RMW. */
        assert(acpi_event_io_write(&p,0x1802,16,v)==!(v&~0x120u));
    }
    for (unsigned v=0;v<256;v++) {
        assert(acpi_event_io_write(&p,0x1889,8,v));
        assert(acpi_event_io_write(&p,0x1899,8,v)==!(v&~0x40u));
        assert(acpi_event_io_write(&p,0x1898,8,v)==!v);
        assert(acpi_event_io_write(&p,0xb2,8,v)==(v==0xf0 || v==0xf1));
    }
    assert(!acpi_event_io_write(&p,0x1800,16,0x10000));
    assert(!acpi_event_io_write(&p,0x1889,8,256));
    assert(!acpi_event_io_write(NULL,0x1889,8,0));
    assert(!acpi_event_io_map(&p,0x1800,4)); /* Cannot straddle status/enable. */
    assert(acpi_event_io_map(&p,0x1800,2));
    assert(!acpi_event_io_map(&p,0x1880,32));
    assert(!acpi_event_io_map(&p,0x1801,2));
    assert(!acpi_event_io_map(&p,0,0));
    assert(!acpi_event_io_map(&p,UINT64_MAX,1));
    assert(!acpi_event_io_map(&p,0x1800,UINT64_MAX));
    assert(!acpi_event_io_read(&p,0x66,8));
    assert(!acpi_event_io_write(&p,0xcf9,8,6));
    /* Exhaust every 16-bit starting register, plus every single-bit mutation of
     * its only admissible command. In particular, never replay SLP_EN, change
     * SLP_TYP/BM_RLD/SCI_EN/reserved bits, or allow notification in legacy mode. */
    for (unsigned before=0;before<65536;before++) {
        unsigned command=(before & ~0x2004u) | 4u;
        bool enabled=(before&1)!=0;
        assert(acpi_event_io_global_release(&p,0x1804,16,before,command)==enabled);
        assert(!acpi_event_io_write(&p,0x1804,16,command));
        for(unsigned bit=0;bit<16;bit++)
            assert(!acpi_event_io_global_release(&p,0x1804,16,before,command^(1u<<bit)));
    }
    assert(!acpi_event_io_global_release(NULL,0x1804,16,1,5));
    assert(!acpi_event_io_global_release(&p,0x1805,16,1,5));
    assert(!acpi_event_io_global_release(&p,0x1802,16,1,5));
    assert(!acpi_event_io_global_release(&p,0,16,1,5));
    assert(!acpi_event_io_global_release(&p,UINT64_MAX,16,1,5));
    for(unsigned bits=0;bits<=64;bits++)
        assert(acpi_event_io_global_release(&p,0x1804,bits,1,5)==(bits==16));
    assert(!acpi_event_io_global_release(&p,0x1804,16,0x10001,5));
    assert(!acpi_event_io_global_release(&p,0x1804,16,1,0x10005));
    /* A fresh change to readable state invalidates a stale requested value. */
    assert(!acpi_event_io_global_release(&p,0x1804,16,3,5));
    for (unsigned n=0;n<sizeof(fa);n++)
        assert(acpi_event_resources_parse(&p.resources,fa,n)!=ACPI_EC_RESOURCES_OK);
    fa[9]++;assert(acpi_event_resources_parse(&p.resources,fa,sizeof(fa))==ACPI_EC_RESOURCES_TABLE);
#define BAD32(off,v) do { init();put32(off,v);rejected(); } while(0)
#define BAD8(off,v) do { init();fa[off]=v;rejected(); } while(0)
    BAD8(88,8);BAD8(89,4);BAD8(90,2);BAD8(91,3);BAD8(46,0);
    BAD8(114,0x10);BAD32(60,0x1820);BAD32(164,0x1820);BAD32(188,0x1820);
    BAD32(152,0x1700);BAD8(148,0);BAD8(149,16);BAD8(150,1);BAD8(151,2);
    BAD32(56,0xffff);BAD32(152,0xffff);
    BAD8(92,31);BAD8(92,66);BAD8(90,0);BAD8(91,0);
    BAD32(48,0);BAD8(52,0);BAD8(53,0xf0);
    /* Overlap with distinct event domains, including PM2 and timer. */
    init();put32(72,0x1803);put32(200,0x1803);rejected();
    init();put32(76,0x1884);put32(212,0x1884);rejected();
    init();put32(48,0x1808);rejected();
    /* Optional blocks may be absent only when length and address agree. */
    init();put32(72,0);put32(200,0);fa[90]=0;
    put32(76,0);put32(212,0);fa[91]=0;assert(parse());
    assert(!acpi_event_io_read(&p,0x1850,8) && !acpi_event_io_read(&p,0x1808,32));
    init();put32(56,0);put32(64,0);assert(parse()); /* Extended-only. */
    init();put32(84,0x1900);fa[93]=16;fa[94]=128;gas(232,0x1900,128,1);assert(parse());
    assert(p.resources.gpe[1].enable==0x1908 && p.resources.gpe[1].first==128);
    p.gpe_enable_mask[1][2]=0x80;assert(acpi_event_io_write(&p,0x190a,8,0x80));
    fa[94]=127;rejected();
    init();put32(4,116);fa[9]=0;uint8_t s=0;
    for(unsigned i=0;i<116;i++)s+=fa[i];
    fa[9]=-s;
    assert(acpi_event_resources_parse(&p.resources,fa,116)==ACPI_EC_RESOURCES_OK);
    if (argc==2) {
        FILE *f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));
        long n=ftell(f);assert(n>0 && n<1024*1024);rewind(f);
        uint8_t *b=malloc(n);assert(b && fread(b,1,n,f)==(size_t)n);fclose(f);
        assert(acpi_event_resources_parse(&p.resources,b,n)==ACPI_EC_RESOURCES_OK);free(b);
        const struct acpi_event_resources *r=&p.resources;
        printf("event-audit: sci=%u status=%x enable=%x control=%x pm2=%x timer=%x gpe0=%x/%x bytes=%u gpe1=%x/%x bytes=%u\n",
            r->sci,r->pm1_status,r->pm1_enable,r->pm1_control,r->pm2_control,r->timer,
            r->gpe[0].status,r->gpe[0].enable,r->gpe[0].bytes,r->gpe[1].status,r->gpe[1].enable,r->gpe[1].bytes);
    }
    puts("acpi-event-io: PASS FADT bounds/consistency/overlap, exact register widths/directions, W1C status and exhaustive enable masks");
}
