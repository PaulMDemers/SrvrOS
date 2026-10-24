#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/mt76_bringup.h>

struct fake { unsigned calls,fail,writes,polls,block,latency,remaining; bool stuck,busy,empty; uint32_t unmapped,ctrl; };
static bool step(struct fake *f) { return ++f->calls != f->fail; }
static bool read_reg(void *context,uint32_t address,uint32_t *value) {
    struct fake *f=context;if(!step(f))return false;
    if(address==0x24) {
        if(f->busy) { *value=1u<<30;return true; }
        *value=f->ctrl;
        if(f->ctrl&(1u<<30)) {
            f->polls++;
            if(!f->stuck && !f->remaining) f->ctrl &= ~(1u<<30);
            else if(f->remaining) --f->remaining;
            *value=f->ctrl;
        }
        *value &= ~63u;
        if(f->unmapped&(1u<<f->block)) *value |= 63;
        return true;
    }
    assert(address>=0x28 && address<=0x34 && !(address&3));
    *value=0;
    for(unsigned i=0;i<4;i++) *value |= (uint32_t)(f->empty?255:(uint8_t)(f->block*16+address-0x28+i+1))<<(8*i);
    return true;
}
static bool write_reg(void *context,uint32_t address,uint32_t value) {
    struct fake *f=context;if(!step(f))return false;
    assert(address==0x24 && !(value&(3u<<6)) && (value&(1u<<30)));
    assert((value & 0x8000ff00u)==0x80003f00u);
    f->block=(value>>16)&0x3ff;assert(f->block==f->writes*16);f->block/=16;
    f->writes++;f->remaining=f->latency;f->ctrl=value;return true;
}
static bool delay(void *context,uint32_t us) {
    assert(us==2 || us==1000);return step(context);
}
static struct fake initial(void) { return (struct fake){.ctrl=0x80003fc0}; }
static void cleared(const struct mt76_efuse_capture *out) {
    for(unsigned i=0;i<512;i++)assert(out->bytes[i]==0);
    assert(!out->unmapped_blocks);
}
static void efuse(void) {
    struct fake f=initial();struct mt76_efuse_capture out;
    struct mt76_identity id={0x76100002,0x76502000};
    struct mt76_register_io io={&f,read_reg,write_reg,delay};
    assert(mt76_capture_efuse(&io,&id,&out)==MT76_CAPTURE_OK);
    assert(f.writes==32 && f.polls==32 && out.unmapped_blocks==0);
    for(unsigned i=0;i<512;i++)assert(out.bytes[i]==(uint8_t)(i+1));
    unsigned total=f.calls;
    for(unsigned n=1;n<=total;n++) {
        f=initial();f.fail=n;memset(&out,0xa5,sizeof out);
        enum mt76_capture_status result=mt76_capture_efuse(&io,&id,&out);
        assert(result==MT76_CAPTURE_IO || result==MT76_CAPTURE_CANCELLED);
        assert(f.calls==n);cleared(&out);
    }
    f=initial();f.busy=true;
    assert(mt76_capture_efuse(&io,&id,&out)==MT76_CAPTURE_BUSY && !f.writes);cleared(&out);
    f=initial();f.stuck=true;
    assert(mt76_capture_efuse(&io,&id,&out)==MT76_CAPTURE_TIMEOUT && f.writes==1 && f.polls==1024);cleared(&out);
    f=initial();f.latency=2;
    assert(mt76_capture_efuse(&io,&id,&out)==MT76_CAPTURE_OK && f.polls==96);
    f=initial();f.latency=40;
    assert(mt76_capture_efuse(&io,&id,&out)==MT76_CAPTURE_TIMEOUT && f.polls==1024 && f.writes==25);cleared(&out);
    f=initial();f.latency=63;
    assert(mt76_capture_efuse(&io,&id,&out)==MT76_CAPTURE_TIMEOUT && f.polls==1024 && f.writes==16);cleared(&out);
    f=initial();f.unmapped=0x80000002;
    assert(mt76_capture_efuse(&io,&id,&out)==MT76_CAPTURE_OK && out.unmapped_blocks==f.unmapped);
    for(unsigned i=16;i<32;i++)assert(out.bytes[i]==255);
    for(unsigned i=496;i<512;i++)assert(out.bytes[i]==255);
    f=initial();f.empty=true;
    assert(mt76_capture_efuse(&io,&id,&out)==MT76_CAPTURE_EMPTY);cleared(&out);
    f=initial();f.unmapped=UINT32_MAX;
    assert(mt76_capture_efuse(&io,&id,&out)==MT76_CAPTURE_EMPTY);cleared(&out);
    f=initial();id.mac++;
    assert(mt76_capture_efuse(&io,&id,&out)==MT76_CAPTURE_REFUSED && !f.calls);cleared(&out);
    assert(mt76_capture_efuse(NULL,&id,&out)==MT76_CAPTURE_REFUSED);
    assert(mt76_capture_efuse(&io,&id,NULL)==MT76_CAPTURE_REFUSED);
}
static void put32(uint8_t *p,uint32_t value) { for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(8*i)); }
static void firmware(void) {
    const unsigned lengths[]={1,3,4,0x38ef,0x38f0,0x38f1,69108,68716};
    for(unsigned n=0;n<sizeof lengths/sizeof *lengths;n++) {
        size_t ilm=lengths[n],dlm=n?11476:0,size=32+64+ilm+dlm;
        uint8_t *image=calloc(size,1);assert(image);put32(image,(uint32_t)(64+ilm));put32(image+4,(uint32_t)dlm);
        size_t got_ilm=0,got_dlm=0,index=0;bool ivb=false;struct mt76_fw_chunk c;
        while(mt76_firmware_chunk(image,size,index,&c)) {
            assert(index++<100 && !ivb);
            assert(c.file_offset+c.bytes<=size);
            if(c.segment==MT76_FW_IVB) {
                assert(got_ilm==ilm && got_dlm==dlm);
                assert(c.file_offset==32 && c.bytes==64 && c.wire_bytes==64);ivb=true;
            } else {
                size_t *got=c.segment==MT76_FW_ILM?&got_ilm:&got_dlm;
                assert(c.destination==(c.segment==MT76_FW_ILM?64:0x80000)+*got);
                assert(c.file_offset==(c.segment==MT76_FW_ILM?96:96+ilm)+*got);
                assert(c.bytes && c.bytes<=0x38f0 && c.padded_bytes>=c.bytes && c.padded_bytes-c.bytes<4);
                assert(!(c.padded_bytes&3) && c.wire_bytes==c.padded_bytes+8 && c.wire_bytes<=0x38f8);
                *got+=c.bytes;
            }
        }
        assert(ivb && !c.bytes && !c.file_offset);
        assert(!mt76_firmware_chunk(image,size,SIZE_MAX,&c));
        assert(!mt76_firmware_chunk(image,size-1,0,&c));
        free(image);
    }
    struct mt76_fw_chunk c;
    size_t big_size=32+0x80001;
    uint8_t *big=calloc(big_size,1);assert(big);put32(big,0x80001);
    assert(!mt76_firmware_chunk(big,big_size,0,&c));free(big);
    big_size=32+65+0x80001;big=calloc(big_size,1);assert(big);put32(big,65);put32(big+4,0x80001);
    assert(!mt76_firmware_chunk(big,big_size,0,&c));free(big);
    assert(!mt76_firmware_chunk(NULL,0,0,&c));
    assert(!mt76_firmware_chunk(NULL,0,0,NULL));
}
struct map_fake {unsigned calls,auth,fail,deny,corrupt,delays,writes,block;bool stuck,unmapped;uint32_t ctrl,ctrl_flip;unsigned ctrl_fault_call;uint8_t bytes[32];};
static bool map_step(struct map_fake *f){return ++f->calls!=f->fail;}
static bool map_auth(void *p){struct map_fake *f=p;return ++f->auth!=f->deny;}
static bool map_read(void *p,uint32_t a,uint32_t *v){
 struct map_fake *f=p;if(!map_step(f))return false;
 if(a==0x1004)*v=0;
 else if(a==0x2000)*v=0xf000f200;
 else if(a==0x208)*v=0x40000050;
 else if(a==0x24){if(!f->stuck)f->ctrl&=~(1u<<30);if(f->unmapped)f->ctrl|=63;*v=f->ctrl;}
 else {assert(a>=0x28 && a<=0x34 && !(a&3));unsigned start=f->block*16+a-0x28;*v=0;for(unsigned j=0;j<4;j++)*v|=(uint32_t)f->bytes[start+j]<<(8*j);}
 if(f->corrupt==f->calls)*v^=1;
 if(a==0x24 && f->ctrl_fault_call==f->calls)*v^=f->ctrl_flip;
 return true;
}
static bool map_write(void *p,uint32_t a,uint32_t v){
 struct map_fake *f=p;if(!map_step(f))return false;
 assert(a==0x24 && f->writes<2 && ((v>>6)&3)==1 && ((v>>16)&0x3ff)==0x1e0+16*f->writes && (v&(1u<<30)));
 assert((v&~((0x3ffu<<16)|(3u<<6)|(1u<<30)))==(f->ctrl&~((0x3ffu<<16)|(3u<<6)|(1u<<30))));
 f->block=f->writes++;f->ctrl=v;return true;
}
static bool map_delay(void *p,uint32_t us){struct map_fake *f=p;assert(us==2 || us==1000);f->delays++;return map_step(f);}
static void map_init(struct map_fake *f){memset(f,0,sizeof *f);f->ctrl=0x8000a500;for(unsigned i=0;i<5;i++)f->bytes[i]=i+1;}
static enum mt76_capture_status map_run(struct map_fake *f,struct mt76_efuse_map *s){
 const struct mt76_identity id={.asic=0x76100002,.mac=0x76502000};const struct mt76_efuse_map_io io={{f,map_read,map_write,map_delay},map_auth};return mt76_capture_efuse_map(&io,&id,s);
}
static void efuse_map(void){
 struct map_fake f;struct mt76_efuse_map s={0};map_init(&f);
 assert(map_run(&f,&s)==MT76_CAPTURE_OK && s.complete && s.valid && s.blocks==2 && s.reads==24 && s.writes==2 && s.polls==2 && s.zero_count==24 && s.free_span==24 && s.reference_used==5 && f.delays==2);
 unsigned calls=f.calls,auth=f.auth;assert(map_run(&f,&s)==MT76_CAPTURE_REFUSED && f.calls==calls);
 for(unsigned n=1;n<=calls;n++){map_init(&f);f.fail=n;s=(struct mt76_efuse_map){0};assert(map_run(&f,&s)!=MT76_CAPTURE_OK && !s.complete && !s.valid && f.calls==n);assert(map_run(&f,&s)==MT76_CAPTURE_REFUSED);}
 for(unsigned n=1;n<=auth;n++){map_init(&f);f.deny=n;s=(struct mt76_efuse_map){0};assert(map_run(&f,&s)==MT76_CAPTURE_CANCELLED && !s.complete && f.auth==n);}
 map_init(&f);f.stuck=true;s=(struct mt76_efuse_map){0};assert(map_run(&f,&s)==MT76_CAPTURE_TIMEOUT && s.polls==1024 && s.writes==1 && !s.complete);
 map_init(&f);f.unmapped=true;s=(struct mt76_efuse_map){0};assert(map_run(&f,&s)==MT76_CAPTURE_EMPTY && !s.complete);
 for(unsigned n=1;n<=3;n++){map_init(&f);f.corrupt=n;s=(struct mt76_efuse_map){0};assert(map_run(&f,&s)==MT76_CAPTURE_BUSY && !s.writes);}
 const unsigned control_calls[]={8,22};
 for(unsigned i=0;i<2;i++)for(unsigned bit=0;bit<32;bit++)if(bit==6 || bit==7 || (bit>=16 && bit<=25) || bit==30){
  map_init(&f);f.ctrl_fault_call=control_calls[i];f.ctrl_flip=1u<<bit;s=(struct mt76_efuse_map){0};assert(map_run(&f,&s)==MT76_CAPTURE_BUSY && !s.complete && f.calls==control_calls[i]);
 }
 uint8_t data[32];memset(data,0,sizeof data);s=(struct mt76_efuse_map){0};assert(!mt76_efuse_map_analyze(data,32,&s) && s.free_span==29);
 memset(data,0xff,sizeof data);assert(!mt76_efuse_map_analyze(data,32,&s));
 memset(data,1,sizeof data);assert(mt76_efuse_map_analyze(data,32,&s) && s.free_span==1 && s.reference_used==28);
 data[0]=data[28]=0;assert(!mt76_efuse_map_analyze(data,32,&s) && s.zero_count==2 && s.free_span==29);
 for(unsigned occupied=0;occupied<=29;occupied++){
  memset(data,0,sizeof data);memset(data,1,occupied);bool ok=mt76_efuse_map_analyze(data,32,&s);assert(ok==(occupied>=5));
  unsigned span=s.free_span;memset(data+29,0xff,3);assert(mt76_efuse_map_analyze(data,32,&s)==ok && s.free_span==span);
 }
 assert(!mt76_efuse_map_analyze(data,31,&s));
 uint32_t command;assert(!mt76_efuse_map_command(UINT32_MAX,0,&command) && !mt76_efuse_map_command(1u<<30,0,&command) && !mt76_efuse_map_command(0,2,&command));
 for(unsigned bit=0;bit<32;bit++)for(unsigned block=0;block<2;block++){
  uint32_t old=1u<<bit;if(bit==30)continue;assert(mt76_efuse_map_command(old,block,&command));
  assert(((command>>6)&3)==1 && ((command>>16)&0x3ff)==0x1e0+16*block && (command&(1u<<30)));
  assert(!((old^command)&~((0x3ffu<<16)|(3u<<6)|(1u<<30))));
 }
 puts("EFUSE map: PASS physical mode1/two fixed blocks, span boundaries, padding exclusion, no programming modes, IO/cancellation/guard failures, timeout and no retry");
}
struct gated_map {struct map_fake f;struct mt76_efuse_map_gate g;};
static bool gated_read(void *p,uint32_t a,uint32_t *v){struct gated_map *f=p;*v=0;if(!mt76_efuse_map_begin(&f->g,false,a,0))return false;bool ok=map_read(&f->f,a,v);return mt76_efuse_map_finish(&f->g,ok,*v);}
static bool gated_write(void *p,uint32_t a,uint32_t v){struct gated_map *f=p;if(!mt76_efuse_map_begin(&f->g,true,a,v))return false;return mt76_efuse_map_finish(&f->g,map_write(&f->f,a,v),v);}
static bool gated_delay(void *p,uint32_t us){struct gated_map *f=p;return mt76_efuse_map_delay(&f->g,us) && map_delay(&f->f,us);}
static bool gated_auth(void *p){struct gated_map *f=p;return !f->g.failed && map_auth(&f->f);}
static enum mt76_capture_status gated_run(struct gated_map *f,struct mt76_efuse_map *s){
 const struct mt76_identity id={.asic=0x76100002,.mac=0x76502000};const struct mt76_efuse_map_io io={{f,gated_read,gated_write,gated_delay},gated_auth};return mt76_capture_efuse_map(&io,&id,s);
}
static void efuse_gate(void){
 struct gated_map f={0};map_init(&f.f);struct mt76_efuse_map s={0};
 assert(gated_run(&f,&s)==MT76_CAPTURE_OK && f.g.phase==15 && f.g.block==2 && f.g.submitted==26 && f.g.polls==2 && !f.g.pending && !f.g.failed);
 unsigned calls=f.f.calls,auth=f.f.auth;
 for(unsigned i=0;i<4;i++)assert(f.g.mac[i]==0 && f.g.core[i]==0xf000f200);
 for(unsigned i=0;i<2;i++)assert(f.g.dma[i]==0x40000050);
 assert(!mt76_efuse_map_begin(&f.g,false,0x1004,0));
 for(unsigned n=1;n<=calls;n++){f=(struct gated_map){0};map_init(&f.f);f.f.fail=n;s=(struct mt76_efuse_map){0};assert(gated_run(&f,&s)!=MT76_CAPTURE_OK && !s.complete && f.f.calls==n);}
 for(unsigned n=1;n<=auth;n++){f=(struct gated_map){0};map_init(&f.f);f.f.deny=n;s=(struct mt76_efuse_map){0};assert(gated_run(&f,&s)==MT76_CAPTURE_CANCELLED && !s.complete && f.f.auth==n);}
 f=(struct gated_map){0};map_init(&f.f);f.f.stuck=true;s=(struct mt76_efuse_map){0};assert(gated_run(&f,&s)==MT76_CAPTURE_TIMEOUT && f.g.polls==1024 && !s.complete);
 /* Independently reject direction/address changes at every transfer phase. */
 for(unsigned p=0;p<15;p++)if(p!=6 && p!=7){
  uint32_t a=p==0 || p==13?0x1004:p==1 || p==14?0x2000:p==2?0x208:p>=9 && p<=12?0x28+4*(p-9):0x24;
  struct mt76_efuse_map_gate g={.phase=p};assert(!mt76_efuse_map_begin(&g,p!=4,a,0) && g.failed);
  g=(struct mt76_efuse_map_gate){.phase=p};assert(!mt76_efuse_map_begin(&g,p==4,a+1,0));
  g=(struct mt76_efuse_map_gate){.phase=p,.pending=true};assert(!mt76_efuse_map_begin(&g,p==4,a,0));
 }
 for(unsigned b=0;b<2;b++){
  uint32_t command;assert(mt76_efuse_map_command(0x8000a500,b,&command));
  for(unsigned bit=0;bit<32;bit++){struct mt76_efuse_map_gate g={.phase=4,.block=b,.control=0x8000a500};assert(!mt76_efuse_map_begin(&g,true,0x24,command^(1u<<bit)) && g.failed);}
 }
 struct mt76_efuse_map_gate g={0};assert(!mt76_efuse_map_finish(&g,true,0));
 g=(struct mt76_efuse_map_gate){.phase=5,.polls=1024};assert(!mt76_efuse_map_begin(&g,false,0x24,0));
 for(unsigned p=0;p<=15;p++){g=(struct mt76_efuse_map_gate){.phase=p};assert(!mt76_efuse_map_delay(&g,3));}
 puts("EFUSE native gate: PASS ordered sequence, single submission, exact preserved command, faults, cancellation, timeout, guard receipts");
}
int main(void) { efuse_gate();efuse_map();efuse();firmware();puts("mt76-bringup: PASS logical capture, per-operation faults, bounded timeout, private-output clearing, firmware chunk boundaries"); }
