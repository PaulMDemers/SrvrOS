#include <srvros/intel_display_backing.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {struct intel_capture_result c;struct intel_backing_endpoint e;unsigned endpoints,maps,unmaps,words,ptes,clocks,batches,fail_word,batch_fault;uint32_t page;bool mapped,fail_map,fail_unmap,change,change_fence,change_endpoint,freeze,reverse;uint64_t time;};
static bool batched;
static struct intel_backing_result result;
static bool endpoint(void *ctx,struct intel_backing_endpoint *e){struct fake *f=ctx;*e=f->e;if(f->change_endpoint && ++f->endpoints>2)e->config[2]^=0x100000;return true;}
static bool map(void *ctx,uint32_t off){struct fake *f=ctx;assert(!f->mapped && !(off&4095));if(f->fail_map)return false;f->mapped=true;f->page=off;f->maps++;return true;}
static bool unmap(void *ctx){struct fake *f=ctx;assert(f->mapped);if(f->fail_unmap)return false;f->unmaps++;f->mapped=false;return true;}
static bool read(void *ctx,uint32_t off,uint32_t *v){
 struct fake *f=ctx;assert(f->mapped && (off&~4095u)==f->page);f->words++;*v=0;
 if(f->fail_word && f->words==f->fail_word)return false;
 if(off>=0x45400 && off<=0x4540c)*v=0xc0000000;
 else if(off==0x6001c)*v=f->c.sample[1].source[0];
 else if(off==0x70180)*v=f->c.sample[1].pipe[0][2];
 else if(off==0x70188)*v=6144;
 else if(off==0x7019c || off==0x701ac)*v=f->c.sample[1].pipe[0][4];
 else if(off>=0x100000 && off<0x100100){
  unsigned i=(off-0x100000)/8;uint64_t base=f->c.sample[1].pipe[0][4];uint64_t fence=i?0:((uint64_t)(5554176+base-4096+47)<<32)|base|1;
  if(f->change_fence && f->ptes)fence^=1;
  *v=(uint32_t)(fence>>((off&4)?32:0));
 } else if(off>=0x800000 && off<0x1000000){
  unsigned i=(off-0x800000)/8;uint64_t pte=0x90000001ull+4096ull*i;
  if(f->change && f->ptes>=1356*3)pte+=4096;
  *v=(uint32_t)(pte>>((off&4)?32:0));f->ptes++;
 }
 return true;
}
static uint64_t now(void *ctx){struct fake *f=ctx;f->clocks++;if(f->reverse && f->clocks>5)return 0;if(!f->freeze)f->time+=10;return f->time;}
static struct fake fresh(void){
 struct fake f={0};f.c.status=INTEL_CAPTURE_OBSERVED;f.c.active_planes=1;
 f.c.after=(struct intel_capture_identity){.id=0x16268086,.class_rev=0x03000009,.subsystem=0x011b106b,.pch=0x9cc38086,.command=7,.power_valid=1,.firmware_match=1,.mmio_safe=1,.bar={0xc0000004,0,0xb000000c,0}};
 f.c.fb=(struct intel_capture_fb){0xb0000000,1440,900,6144,32,1,8,16,8,8,8,0};
 for(unsigned i=0;i<4;i++)f.c.sample[1].well_after[i]=0xc0000000;
 f.c.sample[1].source[0]=0x059f0383;f.c.sample[1].pipe[0][2]=0xd8000400;f.c.sample[1].pipe[0][3]=6144;
 f.e.identity=f.c.after;uint32_t cfg[]={0x16048086,0x140,0x90000001,0x8fe00001,0x92000001,0x140,0x90000001,1};memcpy(f.e.config,cfg,sizeof(cfg));
 f.e.fb_pages=1350;f.e.fb_entry_base=0xb0000000;f.e.fb_entry_length=0x10000000;f.e.fb_entry_type=7;return f;
}
static bool pairs(void *ctx,uint32_t offset,unsigned count,uint64_t *out,unsigned *reads){
 struct fake *f=ctx;uint32_t first=0x800000+f->c.sample[1].pipe[0][4]/4096*8;
 assert(f->mapped && count && count<=64 && !(offset&7) && (offset&4095)+8*count<=4096 && offset>=first && offset+count*8<=first+1356*8);f->batches++;*reads=0;
 for(unsigned i=0;i<count;i++){
  uint32_t hi,lo,again;
  if(!read(ctx,offset+8*i+4,&hi))return false;
  ++*reads;if(!read(ctx,offset+8*i,&lo))return false;
  ++*reads;if(!read(ctx,offset+8*i+4,&again))return false;
  ++*reads;if(hi!=again)return false;out[i]=((uint64_t)hi<<32)|lo;
 }
 if(f->batch_fault==1)++*reads;
 if(f->batch_fault==2)--*reads;
 if(f->batch_fault==3)f->time+=100001;
 if(f->batch_fault==4)out[count-1]=UINT64_MAX;
 return true;
}
static bool run(struct fake *f){const struct intel_backing_io io={f,endpoint,map,unmap,read,now};return batched?intel_display_backing_batched(&result,&f->c,&io,pairs):intel_display_backing(&result,&f->c,&io);}
int main(void){
 for(unsigned offset=0;offset<3;offset++) {
  struct intel_backing_result scalar;struct fake f=fresh();
  unsigned pages=(unsigned[]){0,3,511}[offset];f.c.sample[1].pipe[0][4]=f.c.sample[1].pipe[0][6]=4096*pages;f.c.fb.physical+=4096*pages;
  batched=false;assert(run(&f));scalar=result;
  f=fresh();f.c.sample[1].pipe[0][4]=f.c.sample[1].pipe[0][6]=4096*pages;f.c.fb.physical+=4096*pages;
  batched=true;assert(run(&f));
  assert(result.count==scalar.count && result.maps==scalar.maps && result.status==scalar.status && result.fence_matches==scalar.fence_matches);
  for(unsigned n=0;n<2;n++) {
   assert(result.sample[n].reads==scalar.sample[n].reads && result.sample[n].hash==scalar.sample[n].hash);
   assert(!memcmp(result.sample[n].fence,scalar.sample[n].fence,sizeof(scalar.sample[n].fence)));
  }
  assert(!memcmp(result.pte,scalar.pte,sizeof(scalar.pte)));
 }
 for(unsigned mode=0;mode<2;mode++) {batched=mode;
 struct fake f=fresh();assert(run(&f));assert(result.status==1 && result.count==1356 && result.bytes==5554176 && result.tiled && result.fence_matches==1 && !result.retained);
 assert(f.maps==f.unmaps && !f.mapped && result.sample[0].present==1356 && result.sample[0].stolen==1356 && result.sample[0].contiguous);
 assert(result.sample[0].hash==result.sample[1].hash && result.sample[0].reads==3*1356+212);
 assert(f.batches==(batched?44u:0u));
 for(unsigned k=0;k<20;k++){
  f=fresh();switch(k){case 0:f.c.status=INTEL_CAPTURE_INVALID;break;case 1:f.c.active_planes=3;break;case 2:f.c.after.id++;break;
  case 3:f.c.after.bar[2]|=0x1000;break;case 4:f.c.fb.physical+=4096;break;case 5:f.c.sample[1].pipe[0][2]|=1u<<15;break;
  case 6:f.c.sample[1].pipe[0][5]=1;break;case 7:f.c.fb.pitch=6145;break;case 8:f.c.fb.height=4096;f.c.sample[1].source[0]=0x059f0fff;break;
  case 9:f.e.config[0]++;break;case 10:f.e.config[1]&=~0xc0;break;case 11:f.e.config[5]++;break;case 12:f.e.config[7]=0;break;
  case 13:f.e.config[3]=f.e.config[2];break;case 14:f.e.fb_entry_type=0;break;case 15:f.e.fb_entry_length=1;break;
  case 16:f.c.sample[1].pipe[0][2]|=1u<<9;break;case 17:f.e.config[1]=f.e.config[5]=0x1140;break;case 18:f.e.identity.command=0;break;case 19:f.c.retained=1;break;}
  assert(!run(&f) && result.status==2 && !f.maps && !f.words);
 }
 f=fresh();f.fail_map=true;assert(!run(&f) && !f.maps && !result.retained);
 f=fresh();f.fail_unmap=true;assert(!run(&f) && result.status==6 && result.retained && f.maps==1 && f.mapped);
 f=fresh();f.change=true;assert(!run(&f) && result.status==7 && f.maps==f.unmaps);
 f=fresh();f.change_fence=true;assert(!run(&f) && result.status==3 && f.maps==f.unmaps);
 f=fresh();f.change_endpoint=true;assert(!run(&f) && result.status==3 && f.maps==f.unmaps);
 f=fresh();f.freeze=true;assert(!run(&f) && result.status==5 && f.maps==f.unmaps);
 f=fresh();f.reverse=true;assert(!run(&f) && result.status==5 && f.maps==f.unmaps);
 /* First/last load failures across full, final-short and page-boundary groups
  * must remain failures and retire the owned mapping in both implementations. */
 for(unsigned word=117;word<3*1356+212;word+=31){f=fresh();f.fail_word=word;assert(!run(&f) && f.maps==f.unmaps && !f.mapped && !result.retained);}
 if(batched)for(unsigned fault=1;fault<=4;fault++){f=fresh();f.batch_fault=fault;assert(!run(&f) && f.maps==f.unmaps && !f.mapped);}
 }
 puts("intel-display-backing: PASS tiled bounds, raw PTE stability, read limits, endpoint refusal, clock and mapping retirement");
}
