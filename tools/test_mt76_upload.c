#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/mt76_bringup.h>
struct fake { unsigned calls,fail,fce,bulk,activation,polls,delay; bool short_bulk,short_ivb,stuck,ones; uint32_t descriptor; };
static uint8_t image[104],scratch[MT76_FW_PACKET_MAX];
static bool step(struct fake *f) { return ++f->calls!=f->fail; }
static bool auth(void *p,const uint8_t *b,size_t n,const struct mt76_identity *id) {
    assert(b==image && n==sizeof image && id->asic==0x76100002);return step(p);
}
static bool fce(void *p,uint32_t reg,uint32_t val) {
    struct fake *f=p;if(!step(f))return false;
    unsigned i=f->fce++;
    assert(reg==(i%2?0x234:0x230));
    assert(val==(i%2?((i<2?4u:8u)<<16):(i<2?64u:0x80000u)));
    return true;
}
static bool readreg(void *p,uint32_t reg,uint32_t *val) {
    struct fake *f=p;if(!step(f))return false;
    if(reg==0x9a8) { assert(f->bulk>0);*val=f->descriptor; }
    else { assert(reg==0x730 && f->activation==1);f->polls++;*val=f->ones?0xffffffffu:(!f->stuck && f->polls>=3); }
    return true;
}
static bool writereg(void *p,uint32_t reg,uint32_t val) {
    struct fake *f=p;if(!step(f))return false;
    assert(reg==0x9a8 && val==f->descriptor+1);f->descriptor=val;return true;
}
static int bulk(void *p,const uint8_t *b,size_t n) {
    struct fake *f=p;if(!step(f))return -1;
    unsigned index=f->bulk++;size_t bytes=index?5:3, offset=index?99:96;
    assert(n==(index?16:12) && b[0]==bytes && b[1]==0 && b[2]==0 && b[3]==0x50);
    assert(!memcmp(b+4,image+offset,bytes));
    for(size_t i=4+bytes;i<n;i++)assert(!b[i]);
    return f->short_bulk?(int)n-1:(int)n;
}
static int activate(void *p,const uint8_t *b,size_t n) {
    struct fake *f=p;if(!step(f))return -1;
    assert(f->bulk==2 && f->descriptor==2 && n==64 && !memcmp(b,image+32,64));
    f->activation++;return f->short_ivb?63:64;
}
static bool delay(void *p,uint32_t us) {
    struct fake *f=p;if(!step(f))return false;
    assert(us==(f->activation?1000:5000));f->delay++;return true;
}
static enum mt76_upload_status run(struct fake *f,struct mt76_firmware_session *s) {
    struct mt76_identity id={0x76100002,0x76502000};
    struct mt76_firmware_io io={f,auth,fce,readreg,writereg,bulk,activate,delay};
    return mt76_upload_firmware(s,&io,&id,image,sizeof image,scratch,sizeof scratch);
}
int main(void) {
    image[0]=67;image[4]=5;for(unsigned i=32;i<sizeof image;i++)image[i]=(uint8_t)i;
    struct fake f={0};struct mt76_firmware_session s={0};
    assert(run(&f,&s)==MT76_UPLOAD_OK);unsigned calls=f.calls;
    assert(f.bulk==2 && f.activation==1 && f.polls==3);
    assert(run(&f,&s)==MT76_UPLOAD_REFUSED && f.calls==calls);
    for(unsigned fail=1;fail<=calls;fail++) {
        f=(struct fake){.fail=fail};s=(struct mt76_firmware_session){0};
        assert(run(&f,&s)!=MT76_UPLOAD_OK && f.calls==fail);
        assert(run(&f,&s)==MT76_UPLOAD_REFUSED && f.calls==fail);
    }
    f=(struct fake){.short_bulk=true};s=(struct mt76_firmware_session){0};
    assert(run(&f,&s)==MT76_UPLOAD_IO && !f.activation && !f.descriptor);
    f=(struct fake){.short_ivb=true};s=(struct mt76_firmware_session){0};
    assert(run(&f,&s)==MT76_UPLOAD_IO && !f.polls);
    for(unsigned ones=0;ones<2;ones++) {
        f=(struct fake){.stuck=true,.ones=ones};s=(struct mt76_firmware_session){0};
        assert(run(&f,&s)==MT76_UPLOAD_TIMEOUT && f.polls==1000);
    }
    memset(scratch,0xaa,sizeof scratch);
    assert(!mt76_firmware_packet(image,sizeof image,0,scratch,11) && scratch[0]==0xaa);
    assert(!mt76_firmware_packet(image,sizeof image,2,scratch,sizeof scratch));
    assert(!mt76_firmware_packet(image,sizeof image,SIZE_MAX,scratch,sizeof scratch));
    assert(!mt76_firmware_packet(image,sizeof image,0,image,sizeof image));
    struct mt76_identity id={0x76100002,0x76502000};
    f=(struct fake){0};s=(struct mt76_firmware_session){0};
    struct mt76_firmware_io io={&f,auth,fce,readreg,writereg,bulk,activate,delay};
    assert(mt76_upload_firmware(&s,&io,&id,image,sizeof image,scratch,1)==MT76_UPLOAD_REFUSED);
    assert(mt76_upload_firmware(&s,&io,&id,image,1,scratch,sizeof scratch)==MT76_UPLOAD_REFUSED);
    id.mac=0;
    assert(mt76_upload_firmware(&s,&io,&id,image,sizeof image,scratch,sizeof scratch)==MT76_UPLOAD_REFUSED);
    assert(!s.attempted && !f.calls);
    puts("mt76-upload: PASS packet bytes/padding, ordered FCE/bulk/descriptor/IVB, every-operation failure, no retry, short transfer, readiness budget");
}
