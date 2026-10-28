#define SRVROS_LAPIC_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/arch/x86_64/lapic.c"
static uint32_t registers[1024];
static unsigned reads[4],count;
uint32_t lapic_test_read(unsigned offset) {
    assert(count<4 && offset<sizeof(registers) && !(offset&15));
    reads[count++]=offset;return registers[offset/4];
}
bool vmm_map_page(uint64_t v,uint64_t p,uint64_t f){(void)v;(void)p;(void)f;return false;}
void console_write(const char *s){(void)s;}
void console_printf(const char *s,...){(void)s;}
static bool reference;
static uint64_t ticks;
bool timer_reference_ready(void){return reference;}
uint64_t timer_ticks(void){return ticks;}
uint64_t timer_monotonic_us(void){return 0;}
bool timer_delay_us(uint64_t us){(void)us;return false;}
int main(void) {
    struct lapic_vector_state out={7,123};
    assert(!lapic_vector_snapshot(41,&out) && !out.bits && !out.lint0 && !count);
    lapic=registers;
    assert(!lapic_vector_snapshot(31,&out) && !lapic_vector_snapshot(255,&out));
    assert(!lapic_vector_snapshot(41,0) && !count);
    for(unsigned vector=32;vector<255;vector++)for(unsigned bits=0;bits<8;bits++) {
        unsigned bank=vector/32*16;uint32_t mask=1u<<(vector%32);
        memset(registers,0xff,sizeof(registers));
        registers[(0x100+bank)/4]=(bits&1)?mask:~mask;
        registers[(0x200+bank)/4]=(bits&2)?mask:~mask;
        registers[(0x180+bank)/4]=(bits&4)?mask:~mask;
        registers[0x350/4]=0x10700;
        uint32_t before[1024];memcpy(before,registers,sizeof(before));count=0;
        assert(lapic_vector_snapshot(vector,&out) && out.bits==bits && out.lint0==0x10700);
        assert(count==4 && reads[0]==0x100+bank && reads[1]==0x200+bank && reads[2]==0x180+bank && reads[3]==0x350);
        assert(!memcmp(before,registers,sizeof(before)));
    }
    count=0;assert(!timer_idle_wakeup_ready() && !count);
    idle_timer_count=62500;reference=true;ticks=1;
    registers[0x320/4]=(1u<<17)|32;registers[0x380/4]=62500;
    registers[0x390/4]=1000;registers[0x3e0/4]=3;
    uint32_t before[1024];memcpy(before,registers,sizeof(before));count=0;
    assert(timer_idle_wakeup_ready() && count==4 && !memcmp(before,registers,sizeof(before)));
    unsigned offsets[]={0x320,0x380,0x390,0x3e0};
    for(unsigned i=0;i<4;i++) { uint32_t saved=registers[offsets[i]/4];registers[offsets[i]/4]=0;count=0;
        assert(!timer_idle_wakeup_ready());registers[offsets[i]/4]=saved; }
    reference=false;count=0;assert(!timer_idle_wakeup_ready() && !count);reference=true;
    ticks=0;assert(!timer_idle_wakeup_ready() && !count);ticks=1;
    idle_timer_count=0;assert(!timer_idle_wakeup_ready() && !count);
    puts("lapic-snapshot: PASS all supported vectors, bit combinations, exact four reads, unavailable output clearing and no register changes");
}
