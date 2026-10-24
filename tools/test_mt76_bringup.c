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
/* Synthetic locally administered address only; no captured identity fixture. */
static const uint8_t address_header[16]={0x10,0x76,1,2,2,0x11,0x22,0x33,0x44,0x55};
struct address_fake {unsigned calls,auth,fail,deny,corrupt;uint32_t words[20];};
static bool address_auth(void *p){struct address_fake *f=p;return ++f->auth!=f->deny;}
static bool address_transfer(void *p,unsigned op,bool write,uint32_t a,uint32_t *v){
 struct address_fake *f=p;unsigned target=op/8,step=op%8;assert(op==f->calls && op<160 && write==(step==4));
 if(++f->calls==f->fail)return false;
 uint32_t expected_a=target<4?0x1008+4*target:0x1090+4*(target-4);
 if(step==0 || step==6){assert(a==0x1004);*v=0;}
 else if(step==1 || step==7){assert(a==0x2000);*v=0xf000f200;}
 else if(step==2){assert(a==0x208);*v=0x40000050;}
 else {assert(a==expected_a);if(write)f->words[target]=*v;else *v=f->words[target];}
 if(f->calls==f->corrupt)*v^=1;
 return true;
}
static void address_init(struct address_fake *f){memset(f,0,sizeof *f);for(unsigned i=0;i<20;i++)f->words[i]=0xa5001234+i;}
static enum mt76_capture_status address_run(struct address_fake *f,struct mt76_mac_address_result *s,const uint8_t *h){
 const struct mt76_efuse_map map={.attempted=true,.complete=true,.valid=true,.blocks=2,.writes=2};
 const struct mt76_mac_address_io io={f,address_auth,address_transfer};return mt76_mac_address_apply(s,&io,&map,h,16);
}
static void mac_address(void){
 struct address_fake f;address_init(&f);struct mt76_mac_address_result s={0};
 assert(address_run(&f,&s,address_header)==MT76_CAPTURE_OK && s.complete && s.operations==160 && s.reads==140 && s.writes==20 && s.verified==20);
 assert(f.words[0]==0x33221102 && f.words[1]==0xff5544 && f.words[2]==0x33221102 && f.words[3]==0x3f5544);
 for(unsigned i=4;i<20;i++)assert(f.words[i]==(i&1?0xa5000000:0));
 assert(address_run(&f,&s,address_header)==MT76_CAPTURE_REFUSED && f.calls==160);
 for(unsigned n=1;n<=160;n++){address_init(&f);f.fail=n;s=(struct mt76_mac_address_result){0};assert(address_run(&f,&s,address_header)==MT76_CAPTURE_IO && !s.complete && f.calls==n);}
 for(unsigned n=1;n<=161;n++){address_init(&f);f.deny=n;s=(struct mt76_mac_address_result){0};assert(address_run(&f,&s,address_header)==MT76_CAPTURE_CANCELLED && !s.complete && f.auth==n);}
 for(unsigned n=1;n<=160;n++)if((n-1)%8!=3 && (n-1)%8!=4){address_init(&f);f.corrupt=n;s=(struct mt76_mac_address_result){0};assert(address_run(&f,&s,address_header)==MT76_CAPTURE_BUSY && !s.complete && f.calls==n);}
 for(unsigned i=0;i<20;i++){address_init(&f);f.words[i]=UINT32_MAX;s=(struct mt76_mac_address_result){0};assert(address_run(&f,&s,address_header)==MT76_CAPTURE_BUSY && !s.complete && f.calls==i*8+4);}
 uint8_t h[16];uint32_t a,v;
 assert(!mt76_mac_address_header_valid(NULL,16) && !mt76_mac_address_header_valid(address_header,15));
 for(unsigned i=0;i<4;i++){memcpy(h,address_header,16);h[i]^=1;assert(!mt76_mac_address_header_valid(h,16));}
 memcpy(h,address_header,16);memset(h+4,0,6);assert(!mt76_mac_address_header_valid(h,16));
 memset(h+4,0xff,6);assert(!mt76_mac_address_header_valid(h,16));
 for(unsigned b=1;b<256;b+=2){memcpy(h,address_header,16);h[4]=(uint8_t)b;assert(!mt76_mac_address_header_valid(h,16));}
 memcpy(h,address_header,16);h[4]=0;assert(mt76_mac_address_header_valid(h,16));
 for(unsigned i=5;i<20;i+=2)for(unsigned bit=0;bit<32;bit++){assert(mt76_mac_address_target(i,address_header,16,1u<<bit,&a,&v));assert(a==0x1090+4*(i-4) && v==((1u<<bit)&0xffff0000));}
 assert(!mt76_mac_address_target(20,address_header,16,0,&a,&v));
 address_init(&f);s=(struct mt76_mac_address_result){0};assert(address_run(&f,&s,NULL)==MT76_CAPTURE_REFUSED && !f.calls);
 const struct mt76_mac_address_io io={&f,address_auth,address_transfer};struct mt76_efuse_map map={0};
 assert(mt76_mac_address_apply(&s,&io,&map,address_header,16)==MT76_CAPTURE_REFUSED && !f.calls);
 puts("private MAC plan: PASS 20 unique targets/160 operations, synthetic address only, exact control fields, high-bit preservation, invalid identity/map, all faults/cancellations/guards, no retry");
}
static bool header_read(void *p,uint32_t a,uint32_t *v){struct gated_map *f=p;*v=0;if(!mt76_mac_header_begin(&f->g,false,a,0))return false;bool ok=map_read(&f->f,a,v);return mt76_mac_header_finish(&f->g,ok,*v);}
static bool header_write(void *p,uint32_t a,uint32_t v){
 struct gated_map *f=p;if(!mt76_mac_header_begin(&f->g,true,a,v))return false;
 bool ok=map_step(&f->f);if(ok){assert(a==0x24 && !(v&((3u<<6)|(0x3ffu<<16))) && (v&(1u<<30)));f->f.ctrl=v;f->f.writes++;}
 return mt76_mac_header_finish(&f->g,ok,v);
}
static void header_init(struct gated_map *f){*f=(struct gated_map){0};map_init(&f->f);memcpy(f->f.bytes,address_header,16);}
static enum mt76_capture_status header_run(struct gated_map *f,struct mt76_mac_header_result *s,uint8_t out[16]){
 const struct mt76_identity id={.asic=0x76100002,.mac=0x76502000};const struct mt76_efuse_map_io io={{f,header_read,header_write,gated_delay},gated_auth};return mt76_capture_mac_header(&io,&id,s,out);
}
static void private_header(void){
 struct gated_map f;header_init(&f);struct mt76_mac_header_result s={0};uint8_t out[16];
 assert(header_run(&f,&s,out)==MT76_CAPTURE_OK && s.complete && s.valid && s.blocks==1 && s.reads==12 && s.writes==1 && s.polls==1 && f.g.phase==15 && f.g.block==1 && f.g.submitted==13 && !memcmp(out,address_header,16));
 unsigned calls=f.f.calls,auth=f.f.auth;
 assert(header_run(&f,&s,out)==MT76_CAPTURE_REFUSED);for(unsigned i=0;i<16;i++)assert(!out[i]);
 for(unsigned n=1;n<=calls;n++){header_init(&f);f.f.fail=n;s=(struct mt76_mac_header_result){0};memset(out,0xaa,16);assert(header_run(&f,&s,out)!=MT76_CAPTURE_OK && !s.complete && f.f.calls==n);for(unsigned i=0;i<16;i++)assert(!out[i]);}
 for(unsigned n=1;n<=auth;n++){header_init(&f);f.f.deny=n;s=(struct mt76_mac_header_result){0};assert(header_run(&f,&s,out)==MT76_CAPTURE_CANCELLED && !s.complete && f.f.auth==n);for(unsigned i=0;i<16;i++)assert(!out[i]);}
 header_init(&f);f.f.stuck=true;s=(struct mt76_mac_header_result){0};assert(header_run(&f,&s,out)==MT76_CAPTURE_TIMEOUT && f.g.polls==1024);
 header_init(&f);f.f.unmapped=true;s=(struct mt76_mac_header_result){0};assert(header_run(&f,&s,out)!=MT76_CAPTURE_OK && !s.complete);for(unsigned i=0;i<16;i++)assert(!out[i]);
 for(unsigned b=0;b<16;b++){
  header_init(&f);memset(f.f.bytes+4,0,6);f.f.bytes[b]^=1;s=(struct mt76_mac_header_result){0};
  if(b>=4 && b<=9 && b!=4)continue;
  assert(header_run(&f,&s,out)!=MT76_CAPTURE_OK && !s.valid);for(unsigned i=0;i<16;i++)assert(!out[i]);
 }
 for(unsigned bit=0;bit<32;bit++){
  uint32_t command;assert(mt76_mac_header_command(0x81e0a540,&command));assert(!(command&((3u<<6)|(0x3ffu<<16))));
  struct mt76_efuse_map_gate g={.phase=4,.control=0x81e0a540};assert(!mt76_mac_header_begin(&g,true,0x24,command^(1u<<bit)));
 }
 puts("private header: PASS fixed logical block0, mode0, bounded polling, mapped valid header, failures clear output, cancellation, no retry");
}
struct gated_address {struct address_fake f;struct mt76_mac_address_gate g;};
static bool private_address_auth(void *p){struct gated_address *f=p;return !f->g.failed && address_auth(&f->f);}
static bool private_address_transfer(void *p,unsigned op,bool w,uint32_t a,uint32_t *v){
 struct gated_address *f=p;if(!mt76_mac_address_begin(&f->g,address_header,op,w,a,*v))return false;
 bool ok=address_transfer(&f->f,op,w,a,v);return mt76_mac_address_finish(&f->g,address_header,ok,*v);
}
static enum mt76_capture_status private_address_run(struct gated_address *f,struct mt76_mac_address_result *s){
 const struct mt76_efuse_map map={.attempted=true,.complete=true,.valid=true,.blocks=2,.writes=2};
 const struct mt76_mac_address_io io={f,private_address_auth,private_address_transfer};return mt76_mac_address_apply(s,&io,&map,address_header,16);
}
static void private_address_gate(void){
 struct gated_address f={0};address_init(&f.f);struct mt76_mac_address_result s={0};
 assert(private_address_run(&f,&s)==MT76_CAPTURE_OK && s.complete && f.g.operations==160 && f.g.submitted==160 && !f.g.failed && !f.g.pending);
 for(unsigned n=1;n<=160;n++){f=(struct gated_address){0};address_init(&f.f);f.f.fail=n;s=(struct mt76_mac_address_result){0};assert(private_address_run(&f,&s)==MT76_CAPTURE_IO && f.f.calls==n && f.g.failed && f.g.failure==MT76_ADDRESS_TRANSPORT && !f.g.compared && !s.complete);}
 for(unsigned n=1;n<=161;n++){f=(struct gated_address){0};address_init(&f.f);f.f.deny=n;s=(struct mt76_mac_address_result){0};assert(private_address_run(&f,&s)==MT76_CAPTURE_CANCELLED && f.f.auth==n && !s.complete);}
 for(unsigned op=0;op<160;op++){
  unsigned step=op%8;uint32_t a,v;assert(mt76_mac_address_target(op/8,address_header,16,0x12345678,&a,&v));
  if(step==0 || step==6)a=0x1004;else if(step==1 || step==7)a=0x2000;else if(step==2)a=0x208;
  struct mt76_mac_address_gate g={.operations=op,.submitted=op,.previous=0x12345678};assert(!mt76_mac_address_begin(&g,address_header,op,step!=4,a,v));
  g=(struct mt76_mac_address_gate){.operations=op,.submitted=op,.previous=0x12345678};assert(!mt76_mac_address_begin(&g,address_header,op,step==4,a+1,v));
  if(step==4)for(unsigned bit=0;bit<32;bit++){g=(struct mt76_mac_address_gate){.operations=op,.submitted=op,.previous=0x12345678};assert(!mt76_mac_address_begin(&g,address_header,op,true,a,v^(1u<<bit)));}
 }
 /* Every readback bit must still reject. Receipts expose only comparison
  * flags and the two public control halfwords, never synthetic MAC bytes. */
 for(unsigned target=0;target<20;target++)for(unsigned bit=0;bit<32;bit++){
  unsigned op=target*8+5;uint32_t a,expected;
  assert(mt76_mac_address_target(target,address_header,16,0xa55a1122,&a,&expected));
  struct mt76_mac_address_gate g={.operations=op,.submitted=op,.previous=0xa55a1122};
  assert(mt76_mac_address_begin(&g,address_header,op,false,a,0));
  uint32_t observed=expected^(1u<<bit),mask=(target==1 || target==3)?0xffff0000u:0;
  assert(!mt76_mac_address_finish(&g,address_header,true,observed));
  assert(g.failed && g.failure==MT76_ADDRESS_MISMATCH && g.operations==op && g.submitted==op+1 && g.compared);
  assert(g.expected_control==(expected&mask) && g.observed_control==(observed&mask));
  assert(g.address_match==((mask&(1u<<bit))!=0) && g.control_match==((mask&(1u<<bit))==0));
  assert(!mt76_mac_address_finish(&g,address_header,true,expected) && g.failure==MT76_ADDRESS_MISMATCH);
 }
 /* Success clears stale comparison receipts before later guards; malformed
  * admission and guard faults remain distinct from a USB failure. */
 for(unsigned step=0;step<8;step++){
  unsigned op=8+step;uint32_t a,v;
  assert(mt76_mac_address_target(1,address_header,16,0,&a,&v));
  if(step==0 || step==6)a=0x1004;else if(step==1 || step==7)a=0x2000;else if(step==2)a=0x208;
  struct mt76_mac_address_gate g={.operations=op,.submitted=op,.compared=true,.expected_control=0xffffffff,.observed_control=0xffffffff};
  assert(mt76_mac_address_begin(&g,address_header,op,step==4,a,v));
  assert(!g.compared && !g.expected_control && !g.observed_control);
  if(step==4 || step==5)continue;
  enum mt76_mac_address_failure reason=(step==0 || step==6)?MT76_ADDRESS_MAC:(step==1 || step==7)?MT76_ADDRESS_CORE:step==2?MT76_ADDRESS_DMA:MT76_ADDRESS_ALL_ONES;
  assert(!mt76_mac_address_finish(&g,address_header,true,0xffffffff) && g.failure==reason);
 }
 puts("private address gate: PASS exact order/direction/values, all transfer/cancel faults, no replay, independent guards");
}
struct captured_address {struct address_fake f;struct mt76_mac_address_gate g;bool gate,mutate;unsigned target,bit;};
static bool captured_address_auth(void *p){struct captured_address *f=p;return !f->g.failed && address_auth(&f->f);}
static bool captured_address_transfer(void *p,unsigned op,bool w,uint32_t a,uint32_t *v){
 struct captured_address *f=p;
 if(f->gate && !mt76_mac_address_begin(&f->g,address_header,op,w,a,*v))return false;
 bool ok=address_transfer(&f->f,op,w,a,v);
 if(ok && op%8==5){if(op/8==1)*v&=0xffff;if(f->mutate && op/8==f->target)*v^=1u<<f->bit;}
 return f->gate?mt76_mac_address_finish(&f->g,address_header,ok,*v):ok;
}
static enum mt76_capture_status captured_address_run(struct captured_address *f,struct mt76_mac_address_result *s,bool profile){
 const struct mt76_efuse_map map={.attempted=true,.complete=true,.valid=true,.blocks=2,.writes=2};
 const struct mt76_mac_address_io io={f,captured_address_auth,captured_address_transfer};
 return mt76_mac_address_apply_profile(s,&io,&map,address_header,16,profile);
}
static void captured_address_init(struct captured_address *f,bool gate){
 memset(f,0,sizeof *f);address_init(&f->f);f->gate=gate;f->g.captured_readback=true;
}
static void captured_address_profile(void){
 for(unsigned gate=0;gate<2;gate++){
  struct captured_address f;captured_address_init(&f,gate);struct mt76_mac_address_result s={0};
  assert(captured_address_run(&f,&s,true)==MT76_CAPTURE_OK && s.complete && s.operations==160 && s.verified==20);
  assert(f.f.words[1]==0xff5544 && f.f.words[3]==0x3f5544); /* Writes unchanged. */
  for(unsigned target=0;target<20;target++)for(unsigned bit=0;bit<32;bit++){
   captured_address_init(&f,gate);f.mutate=true;f.target=target;f.bit=bit;s=(struct mt76_mac_address_result){0};
   assert(captured_address_run(&f,&s,true)!=MT76_CAPTURE_OK && !s.complete && f.f.calls==target*8+6);
   if(gate)assert(f.g.failure==MT76_ADDRESS_MISMATCH);
  }
  for(unsigned n=1;n<=160;n++){
   captured_address_init(&f,gate);f.f.fail=n;s=(struct mt76_mac_address_result){0};assert(captured_address_run(&f,&s,true)==MT76_CAPTURE_IO && !s.complete && f.f.calls==n);
  }
  for(unsigned n=1;n<=161;n++){
   captured_address_init(&f,gate);f.f.deny=n;s=(struct mt76_mac_address_result){0};assert(captured_address_run(&f,&s,true)==MT76_CAPTURE_CANCELLED && !s.complete && f.f.auth==n);
  }
 }
 /* Legacy write-equals-read semantics must still reject the captured mismatch. */
 struct captured_address f;captured_address_init(&f,false);struct mt76_mac_address_result s={0};
 assert(captured_address_run(&f,&s,false)==MT76_CAPTURE_BUSY && !s.complete && f.f.calls==14);
 /* Both exact expectations reject a widened mask; old ff readback is refused
  * by the new gate, even though it equals the requested write. */
 struct mt76_mac_address_gate g={.captured_readback=true,.operations=13,.submitted=13};
 assert(mt76_mac_address_begin(&g,address_header,13,false,0x100c,0));
 assert(!mt76_mac_address_finish(&g,address_header,true,0xff5544) && g.failure==MT76_ADDRESS_MISMATCH);
 puts("captured MAC profile: PASS unchanged writes, exact zero control readback, every target/bit mismatch, all failures/cancellation, legacy preserved");
}
int main(void) { captured_address_profile();private_header();private_address_gate();mac_address();efuse_gate();efuse_map();efuse();firmware();puts("mt76-bringup: PASS logical capture, per-operation faults, bounded timeout, private-output clearing, firmware chunk boundaries"); }
