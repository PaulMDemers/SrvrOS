/* Production I/O accounting, failure capture and bounded worst retention. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_EXFAT_HOST_TEST
#define SRVROS_EXFAT_COST_TEST
#include "../kernel/src/fs/exfat.c"
static uint64_t clock_us=1000000;static bool fail;static unsigned lines;
uint64_t timer_monotonic_us(void){return clock_us;}
bool block_read(struct block_device *d,uint64_t o,void *p,uint64_t n){(void)d;(void)o;memset(p,0,n);clock_us+=10;return !fail;}
bool block_write(struct block_device *d,uint64_t o,const void *p,uint64_t n){(void)d;(void)o;(void)p;(void)n;clock_us+=20;return !fail;}
struct block_device *block_parent(struct block_device *d){return d;}
static bool flush(struct block_device *d){(void)d;clock_us+=30;return !fail;}
void console_printf(const char *fmt,...){assert(!strncmp(fmt,"exfat-cost",10));lines++;}
int main(void){
 struct block_device d={.flush=flush};struct exfat_volume v={.device=&d,.image_size=4096};unsigned char p[32]={0};
 cost_begin(&v.cost,"write");assert(volume_read(&v,0,p,32));
 v.io_phase=EXFAT_IO_DATA;assert(volume_write(&v,512,p,32));assert(volume_flush(&v));
 v.io_phase=EXFAT_IO_METADATA;assert(write_volume_flags(&v,2));
 assert(v.cost.phase[EXFAT_IO_METADATA].reads==1 && v.cost.phase[0].read_bytes==32 && v.cost.phase[0].read_us==10);
 assert(v.cost.phase[0].writes==1 && v.cost.phase[0].write_bytes==2 && v.cost.phase[0].write_us==20 && v.cost.phase[0].flush_us==30);
 assert(v.cost.phase[2].writes==1 && v.cost.phase[2].write_bytes==32 && v.cost.phase[2].write_us==20 && v.cost.phase[2].flush_us==30);
 cost_end(&v.cost,true);assert(!slow_costs[0].start && !v.cost.active);
 cost_begin(&v.cost,"write");fail=true;assert(!volume_write(&v,512,p,32)&&v.write_blocked);clock_us+=200000;cost_end(&v.cost,false);
 assert(slow_costs[0].start && !slow_costs[0].ok && slow_costs[0].phase[0].write_us==20);
 for(unsigned i=0;i<10;i++){cost_begin(&v.cost,"test");clock_us+=300000+i;cost_end(&v.cost,true);}
 for(unsigned i=0;i<EXFAT_COST_ROWS;i++)assert(slow_costs[i].end-slow_costs[i].start>=300006 && !slow_costs[i].active);
 exfat_print_costs();assert(lines==EXFAT_COST_ROWS*(EXFAT_IO_PHASE_COUNT+1));
 puts("exfat-cost: PASS phase/flag/flush accounting, failure observation, short exclusion and bounded worst retention");
}
