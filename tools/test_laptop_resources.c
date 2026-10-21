#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/laptop_probe.h>
static void checksum(unsigned char *b,size_t n) { b[9]=0;unsigned char sum=0;for(size_t i=0;i<n;i++) sum+=b[i];b[9]=(unsigned char)-sum; }
static void header(unsigned char *b,size_t n,const char *sig) { memcpy(b,sig,4);b[4]=n&255;b[5]=n>>8; }
static void gas(unsigned char *b,unsigned port,unsigned width) { b[0]=1;b[1]=width;b[4]=port&255;b[5]=port>>8; }
static unsigned char *load(const char *dir,const char *name,size_t *size) {
    char path[512];snprintf(path,sizeof(path),"%s/%s.dat",dir,name);FILE *f=fopen(path,"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long length=ftell(f);assert(length>0 && length<=1024*1024);rewind(f);
    unsigned char *b=malloc(length);assert(b && fread(b,1,length,f)==(size_t)length);fclose(f);*size=length;return b;
}
int main(int argc,char **argv) {
    unsigned char ec[83]={0},fa[244]={0};header(ec,sizeof(ec),"ECDT");header(fa,sizeof(fa),"FACP");
    gas(ec+36,0x66,8);gas(ec+48,0x62,8);ec[64]=0x4e;memcpy(ec+65,"\\_SB.PCI0.LPCB.EC",18);
    fa[64]=4;fa[65]=0x18;fa[80]=0x80;fa[81]=0x18;fa[89]=2;fa[92]=32;
    gas(fa+172,0x1804,16);gas(fa+220,0x1880,0);checksum(ec,sizeof(ec));checksum(fa,sizeof(fa));
    struct laptop_resources r;
    assert(laptop_resources_parse(&r,ec,sizeof(ec),fa,sizeof(fa)) && r.gpe_enable==0x1899 && r.gpe_mask==0x40);
    for(unsigned n=0;n<sizeof(ec);n++) assert(!laptop_resources_parse(&r,ec,n,fa,sizeof(fa)));
    ec[36]=0;checksum(ec,sizeof(ec));assert(!laptop_resources_parse(&r,ec,sizeof(ec),fa,sizeof(fa)));ec[36]=1;checksum(ec,sizeof(ec));
    fa[224]=0;checksum(fa,sizeof(fa));assert(!laptop_resources_parse(&r,ec,sizeof(ec),fa,sizeof(fa)));
    assert(!laptop_firmware_match(ec,sizeof(ec)));
    struct laptop_backlight b=laptop_backlight_decode(1u<<31,852,1u<<31,2777u<<16);
    assert(b.mode==1 && b.duty==852 && b.period==2777);
    b=laptop_backlight_decode(0,0,3u<<30,(2777u<<16)|852);assert(b.mode==2 && b.duty==852);
    assert(!laptop_backlight_decode(0,0,0,2777u<<16).mode);
    assert(!laptop_backlight_decode(0,0,3u<<30,1).mode);
    assert(!laptop_backlight_decode(0,0,3u<<30,(100u<<16)|101).mode);
    assert(!laptop_backlight_decode(0,0,UINT32_MAX,UINT32_MAX).mode);
    assert(!laptop_pro_display_match(0x0d26,0x8c4b,ec,sizeof(ec)));
    if(argc==3 && !strcmp(argv[2],"pro")) {
        size_t n;unsigned char *d=load(argv[1],"DSDT",&n);
        assert(laptop_pro_display_match(0x0d26,0x8c4b,d,n));
        assert(!laptop_firmware_match(d,n));
        assert(!laptop_pro_display_match(0x1626,0x8c4b,d,n));
        assert(!laptop_pro_display_match(0x0d26,0x9cc3,d,n));
        assert(!laptop_pro_display_match(0x0d26,0x8c4b,d,n-1));
        d[n-1]^=1;assert(!laptop_pro_display_match(0x0d26,0x8c4b,d,n));
        checksum(d,n);assert(!laptop_pro_display_match(0x0d26,0x8c4b,d,n));free(d);
        puts("laptop-resources: Pro capture matches; wrong chipset, length and modified firmware refused");
    }
    if(argc==2) {
        size_t en,fn,dn;unsigned char *e=load(argv[1],"ECDT",&en),*f=load(argv[1],"FACP",&fn),*d=load(argv[1],"DSDT",&dn);
        assert(laptop_resources_parse(&r,e,en,f,fn) && laptop_firmware_match(d,dn));free(e);free(f);free(d);
        puts("laptop-resources: captured target tables match");
    }
    puts("laptop-resources: PASS table bounds/checksums, resource identity, firmware refusal, CPU/PCH mode and invalid PWM rejection");
}
