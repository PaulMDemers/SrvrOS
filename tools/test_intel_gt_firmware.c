#include <assert.h>
#include <stdio.h>
#include <srvros/intel_gt_firmware.h>
static struct intel_fw_endpoint resource;
static unsigned calls,fail_at,maps,unmaps,reads;static bool mapped,frozen,reverse,change,bad,retire,missing;static uint64_t us;
static bool step(void){return ++calls!=fail_at;}
static bool guard(void *p){assert(!p && !mapped);return step();}
static bool endpoint(void *p,struct intel_fw_endpoint *e){assert(!p && !mapped);if(!step())return false;*e=resource;return true;}
static bool map(void *p,uint64_t physical){assert(!p && !mapped && physical==0x12345000);if(!step())return false;maps++;mapped=true;return true;}
static bool read_word(void *p,uint32_t offset,uint32_t *value){
 assert(!p && mapped);if(!step())return false;reads++;
 const uint32_t words[]={0x65746e49,0x6172476c,0x63696870,0x6d654d73,8,0x02000000,missing?0:7,0,0,0,0,0,0};
 for(unsigned i=0;i<INTEL_FW_WORDS;i++)if(offset==intel_fw_offsets[i]){*value=words[i];if(bad && i==5)*value=0x03000000;if(change && reads>13 && i==7)*value=1;return true;}
 assert(0);return false;
}
static bool unmap(void *p){assert(!p && mapped);unmaps++;if(!step() || retire)return false;mapped=false;return true;}
static uint64_t now(void *p){assert(!p);if(reverse)return us--;if(!frozen)us+=100;return us;}
static void reset(void){calls=fail_at=maps=unmaps=reads=0;mapped=frozen=reverse=change=bad=retire=missing=false;us=1000;resource=(struct intel_fw_endpoint){0x12345000,0x3000,3,0x12345190,0x8000};}
int main(void){
 struct intel_fw_result r;struct intel_fw_io io={0,guard,endpoint,map,read_word,unmap,now};
 reset();assert(intel_fw_observe(&r,&io) && r.status==IFW_OBSERVED && !r.retained && r.maps==2 && reads==26 && !mapped);unsigned total=calls;
 for(unsigned i=1;i<=total;i++){reset();fail_at=i;assert(!intel_fw_observe(&r,&io));assert(!mapped || r.retained);assert(r.status!=IFW_OBSERVED);}
 for(unsigned i=0;i<8;i++){reset();switch(i){case 0:resource.type=0;break;case 1:resource.length=4096;break;case 2:resource.asls=0;break;case 3:resource.asls|=1;break;case 4:resource.asls=0x12345f00;break;case 5:resource.swsci=UINT32_MAX;break;case 6:resource.base++;break;case 7:resource.length=UINT64_MAX;break;}assert(!intel_fw_observe(&r,&io) && !maps && !mapped);}
 reset();missing=true;assert(intel_fw_observe(&r,&io) && reads==14);
 reset();bad=true;assert(!intel_fw_observe(&r,&io) && r.status==IFW_REFUSED && !mapped);
 reset();change=true;assert(!intel_fw_observe(&r,&io) && r.status==IFW_CHANGED && !mapped);
 reset();frozen=true;assert(!intel_fw_observe(&r,&io) && r.status==IFW_CLOCK && !mapped);
 reset();reverse=true;assert(!intel_fw_observe(&r,&io) && r.status==IFW_CLOCK && !mapped);
 reset();retire=true;assert(!intel_fw_observe(&r,&io) && r.retained && mapped && r.status==IFW_RETAINED);
 printf("intel-firmware: PASS bounded RO census, advertised fields only, clocks, retention and %u failure boundaries; no authority\n",total);
}
