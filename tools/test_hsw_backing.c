#include <srvros/haswell_backing.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct hwb_result result;
static struct hwb_endpoint base;
static unsigned maps,unmaps,reads,endpoint_calls,fail_map,fail_read,fail_unmap,change_at;
static uint64_t ticks=1;static unsigned frozen,rollback,late,change_pte;
static unsigned tile_reads,tile_change,tile_value;
static bool live;static uint32_t page;
static bool endpoint(void *ctx,struct hwb_endpoint *e) {(void)ctx;*e=base;endpoint_calls++;if(change_at==endpoint_calls)e->v[HW_DISPARB]^=1;return true;}
static bool map(void *ctx,uint32_t off){(void)ctx;assert(!live);maps++;if(maps==fail_map)return false;live=true;page=off;return true;}
static bool unmap(void *ctx){(void)ctx;assert(live);unmaps++;if(unmaps==fail_unmap)return false;live=false;return true;}
static bool read_word(void *ctx,uint32_t off,uint32_t *out){
 (void)ctx;assert(live && (off&~4095u)==page);reads++;if(reads==fail_read)return false;
 if(off==0x101000){tile_reads++;*out=tile_value^(tile_change && tile_reads==2?1u:0u);}
 else if(off>=0x100000 && off<0x100080){unsigned index=(off-0x100000)/8;*out=index?0:off&4?0x41f02b:1;}
 else {assert(off>=0x200000 && off<0x201800 && !(off&3));unsigned i=(off-0x200000)/4;*out=(0x8da00000+4096*i)|0x805;if(change_pte && maps>4 && i==5)*out^=4096;}
 return true;
}
static uint64_t now(void *ctx){(void)ctx;if(frozen)return ticks;if(rollback && reads>100)return 0;if(late && reads>100)ticks+=HWB_LIMIT_US;return ++ticks;}
static struct hwb_io io={.endpoint=endpoint,.map=map,.unmap=unmap,.read=read_word,.now=now};
static void setup(void){
 base=(struct hwb_endpoint){0};uint64_t *v=base.v;
 v[HW_GPU]=0x0a268086;v[HW_REV]=0x03000009;v[HW_SUB]=0x011a106b;v[HW_HOST]=0x0a048086;v[HW_PCH]=0x9c438086;
 v[HW_MMIO_LO]=0xb0000004;v[HW_AP_LO]=0xa000000c;v[HW_CMD]=7;v[HW_GGC]=v[HW_GPU_GGC]=0x209;
 v[HW_BDSM]=v[HW_GPU_BDSM]=0x8da00001;v[HW_BGSM]=0x8d800001;v[HW_TOLUD]=0x8fa00001;
 v[HW_CONTROL]=v[HW_CONTROL2]=0x98004400;v[HW_STRIDE]=v[HW_PITCH]=5632;v[HW_SOURCE]=(1365u<<16)|767;
 for(unsigned i=HW_WELL0;i<=HW_WELL3;i++)v[i]=0x40000000;
 v[HW_FB_COUNT]=1;v[HW_VA]=0xffff8000a0000000ull;v[HW_PA]=v[HW_MEM_BASE]=0xa0000000;
 v[HW_WIDTH]=1366;v[HW_HEIGHT]=768;v[HW_SPAN]=v[HW_MEM_LEN]=4325376;v[HW_MEM_TYPE]=7;
 v[HW_PAGES]=1056;v[HW_ENTRIES]=44;v[HW_BPP]=32;v[HW_COLOR]=HWB_COLOR;v[HW_CLASS]=1;
 v[HW_CONTIG]=v[HW_MMIO_SAFE]=v[HW_FIRMWARE]=v[HW_STOLEN_RESERVED]=1;
 io.cpu_control=false;tile_reads=tile_change=0;tile_value=8;
 maps=unmaps=reads=endpoint_calls=fail_map=fail_read=fail_unmap=change_at=frozen=rollback=late=change_pte=0;ticks=1;live=false;
}
int main(void){
 setup();assert(hwb_capture(&result,&io));assert(result.status==1 && result.count==1056 && result.maps==8 && maps==unmaps && !live);
 for(unsigned i=0;i<2;i++)assert(result.sample[i].count==1056 && result.sample[i].present==1056 && result.sample[i].stolen==1056 && result.sample[i].reads==1152);
 assert(!memcmp(result.sample[0].pte,result.sample[1].pte,sizeof(result.sample[0].pte)));
 for(unsigned field=0;field<HWB_WORDS;field++) {
  setup();base.v[field]=UINT64_MAX;
  if(field==HW_DISPARB || field==HW_LINOFF)continue;
  assert(!hwb_capture(&result,&io));assert(!maps);
 }
 for(unsigned i=1;i<=8;i++){setup();fail_map=i;assert(!hwb_capture(&result,&io));assert(result.status==4 && !live);}
 for(unsigned i=1;i<=8;i++){setup();fail_unmap=i;assert(!hwb_capture(&result,&io));assert(result.status==6 && result.retained && live && unmaps==i);}
 for(unsigned i=1;i<=4;i++){setup();change_at=i;assert(!hwb_capture(&result,&io));assert(!live);}
 for(unsigned i=1;i<=2304;i+=31){setup();fail_read=i;assert(!hwb_capture(&result,&io));assert(result.status==4 && !live);}
 setup();change_pte=1;assert(!hwb_capture(&result,&io));assert(result.status==7 && !live);
 setup();frozen=1;assert(!hwb_capture(&result,&io));assert(result.status==5 && !live);
 setup();rollback=1;assert(!hwb_capture(&result,&io));assert(result.status==5 && !live);
 setup();late=1;assert(!hwb_capture(&result,&io));assert(result.status==5 && !live);
 setup();base.v[HW_GGC]=base.v[HW_GPU_GGC]=0x1e0;assert(!hwb_capture(&result,&io));assert(!maps);
 setup();base.v[HW_GGC]=base.v[HW_GPU_GGC]=0x309;assert(!hwb_capture(&result,&io));assert(!maps);
 setup();base.v[HW_WIDTH]=384;base.v[HW_HEIGHT]=4096;base.v[HW_STRIDE]=base.v[HW_PITCH]=1536;
 base.v[HW_SPAN]=base.v[HW_MEM_LEN]=1536*4096;base.v[HW_PAGES]=1536;base.v[HW_SOURCE]=(383u<<16)|4095;
 assert(hwb_capture(&result,&io) && result.count==1536 && result.sample[1].count==1536 && result.sample[1].stolen==1536);
 setup();io.cpu_control=true;assert(hwb_capture(&result,&io));
 assert(result.version==2 && maps==10 && unmaps==10 && tile_reads==2 && !live);
 for(unsigned i=0;i<2;i++)assert(result.sample[i].tilectl==8 && result.sample[i].reads==1153);
 for(unsigned i=1;i<=10;i++){setup();io.cpu_control=true;fail_map=i;assert(!hwb_capture(&result,&io));assert(!live);}
 for(unsigned i=1;i<=10;i++){setup();io.cpu_control=true;fail_unmap=i;assert(!hwb_capture(&result,&io));assert(result.status==6 && result.retained && live);}
 setup();io.cpu_control=true;tile_change=1;assert(!hwb_capture(&result,&io) && result.status==3 && !live);
 setup();io.cpu_control=true;tile_value=UINT32_MAX;assert(!hwb_capture(&result,&io) && result.status==4 && !live);
 setup();io.cpu_control=true;fail_read=1153;assert(!hwb_capture(&result,&io) && result.status==4 && !live);
 setup();io.cpu_control=true;frozen=1;assert(!hwb_capture(&result,&io) && result.status==5 && !live);
 setup();io.cpu_control=true;base.v[HW_WIDTH]=384;base.v[HW_HEIGHT]=4096;base.v[HW_STRIDE]=base.v[HW_PITCH]=1536;
 base.v[HW_SPAN]=base.v[HW_MEM_LEN]=1536*4096;base.v[HW_PAGES]=1536;base.v[HW_SOURCE]=(383u<<16)|4095;
 assert(hwb_capture(&result,&io) && result.count==1536 && maps==10 && tile_reads==2);
 setup();io.cpu_control=true;uint64_t offset=1023*4096;
 base.v[HW_SURF]=base.v[HW_SURF2]=base.v[HW_LIVE]=offset;base.v[HW_VA]+=offset;
 base.v[HW_PA]+=offset;base.v[HW_MEM_BASE]+=offset;
 assert(!hwb_capture(&result,&io) && result.status==2 && !maps && !tile_reads);
 puts("hsw-backing: PASS exact profile/cache bits/full vectors, map/read/retirement faults, endpoint/PTE changes and bounded clocks");
}
