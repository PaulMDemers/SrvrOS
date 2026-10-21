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
int main(void) { efuse();firmware();puts("mt76-bringup: PASS logical capture, per-operation faults, bounded timeout, private-output clearing, firmware chunk boundaries"); }
