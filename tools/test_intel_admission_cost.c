#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <srvros/intel_display_backing.h>
static bool admission_active;
static char output[512];
static void console_printf(const char *fmt,...){va_list ap;va_start(ap,fmt);assert(vsnprintf(output,sizeof(output),fmt,ap)>0);va_end(ap);}
#include "../kernel/src/drivers/intel_admission_cost.inc"
static unsigned decode(uint8_t *raw){
 const char *p=strstr(output," b=");assert(p);p+=3;unsigned bits=0,value=0,n=0;
 for(;*p && *p!='=' && *p!='\n';p++){
  const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",*at=strchr(alphabet,*p);assert(at);
  value=(value<<6)|(unsigned)(at-alphabet);bits+=6;if(bits>=8){bits-=8;raw[n++]=(uint8_t)(value>>bits);}
 }assert(n==76);return n;
}
static uint64_t word(const uint8_t *p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static struct intel_backing_result sample(uint64_t begin,uint64_t end){
 struct intel_backing_result r={.status=1,.count=1356};
 r.sample[0]=(struct intel_backing_sample){.begin_us=begin,.end_us=begin+1,.ptes=1356,.reads=4280};
 r.sample[1]=(struct intel_backing_sample){.begin_us=begin+1,.end_us=end,.ptes=1356,.reads=4280};return r;
}
int main(void){
 admission_cost_reset();struct intel_backing_result r=sample(100,200);admission_cost_record(&r,true);assert(!admission_cost_capture.count);
 admission_active=true;admission_cost_record(&r,true);r=sample(300,350);admission_cost_record(&r,true);r=sample(400,500);admission_cost_record(&r,true);r=sample(700,800);admission_cost_record(&r,true);
 admission_cost_report(250,600);uint8_t raw[80];decode(raw);assert(word(raw,8)==250 && word(raw+8,8)==600 && !word(raw+16,4));
 assert(word(raw+20,4)==4 && word(raw+24,4)==4*8560 && word(raw+28,4)==4*2712 && !word(raw+32,4));
 assert(word(raw+36,4)==350 && word(raw+40,4)==350 && word(raw+44,4)==100);
 assert(word(raw+48,4)==2 && word(raw+52,4)==2*8560 && word(raw+56,4)==2*2712 && !word(raw+60,4));
 assert(word(raw+64,4)==150 && word(raw+68,4)==150 && word(raw+72,4)==100);
 admission_cost_reset();assert(!admission_cost_capture.count && !admission_cost_capture.flags);
 r=sample(100,200);r.status=4;r.sample[1].end_us=0;admission_cost_record(&r,false);admission_cost_report(0,0);decode(raw);assert(word(raw+20,4)==1 && word(raw+32,4)==1 && !word(raw+36,4));
 admission_cost_reset();r=sample(100,200);admission_cost_record(&r,true);r=sample(150,250);admission_cost_record(&r,true);assert(admission_cost_capture.flags==2 && !admission_cost_capture.row[1].status);
 admission_cost_reset();r=sample(100,300101);admission_cost_record(&r,true);assert(admission_cost_capture.flags==2);
 admission_cost_reset();for(unsigned i=0;i<513;i++){r=sample(100+4*i,103+4*i);admission_cost_record(&r,true);}
 assert(admission_cost_capture.count==512 && admission_cost_capture.flags==1);admission_cost_report(100,2200);decode(raw);assert(word(raw+16,4)==1 && word(raw+20,4)==512);
 puts("intel-admission-cost: PASS existing clocks only, wire counts/sums/subsets, inactive/reset, partial failure, ordering and bounded overflow");
}
