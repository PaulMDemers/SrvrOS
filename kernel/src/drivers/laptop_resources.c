#include <srvros/laptop_probe.h>
static uint32_t le32(const uint8_t *b) { return b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24; }
static bool equal(const uint8_t *a,const char *b,size_t n) { for(size_t i=0;i<n;i++) if(a[i]!=(uint8_t)b[i]) return false;return true; }
static bool table(const uint8_t *b,size_t size,const char *sig,size_t minimum) {
    if(!b || size<minimum || size>1024*1024 || !equal(b,sig,4) || le32(b+4)!=size) return false;
    uint8_t sum=0;for(size_t i=0;i<size;i++) sum+=b[i];return !sum;
}
static bool gas(const uint8_t *b,uint16_t port,unsigned width) {
    if(b[0]!=1 || b[1]!=width || b[2] || b[3]>2 || (b[4]|(uint16_t)b[5]<<8)!=port) return false;
    for(unsigned i=6;i<12;i++) if(b[i]) return false;
    return true;
}
bool laptop_resources_parse(struct laptop_resources *out,const uint8_t *ec,size_t en,const uint8_t *fa,size_t fn) {
    if(!out || !table(ec,en,"ECDT",83) || !table(fa,fn,"FACP",244) || en!=83 ||
       !gas(ec+36,0x66,8) || !gas(ec+48,0x62,8) || le32(ec+60) || ec[64]!=0x4e ||
       !equal(ec+65,"\\_SB.PCI0.LPCB.EC",18) || le32(fa+64)!=0x1804 ||
       le32(fa+68) || fa[89]!=2 || le32(fa+80)!=0x1880 || fa[92]!=32 ||
       !gas(fa+172,0x1804,16) || !gas(fa+220,0x1880,0)) return false;
    *out=(struct laptop_resources){0x66,0x62,0x1804,0x1899,0x40};return true;
}
bool laptop_firmware_match(const uint8_t *b,size_t n) {
    if(!table(b,n,"DSDT",36) || n!=35034) return false;
    uint64_t hash=14695981039346656037ull;
    for(size_t i=0;i<n;i++) hash=(hash^b[i])*1099511628211ull;
    /* Exact captured table identity, not a security/authentication primitive. */
    return hash==0xe0f810cb41f75c0bull;
}
struct laptop_backlight laptop_backlight_decode(uint32_t cpu,uint32_t cd,uint32_t pch,uint32_t pd) {
    struct laptop_backlight b={0};
    if(!(pch&(1u<<31)) || cpu==UINT32_MAX || pch==UINT32_MAX || !((pd>>16)&65535)) return b;
    b.period=pd>>16;
    if(pch&(1u<<30)) { b.mode=2;b.duty=pd&65535; }
    else if(cpu&(1u<<31)) { b.mode=1;b.duty=cd&65535; }
    if(!b.mode || b.duty>b.period) return (struct laptop_backlight){0};
    return b;
}

/* Read-only display diagnostic identity captured on the local Pro11,2.
 * This does not authorize EC access or a brightness provider. */
bool laptop_pro_display_match(uint16_t gpu,uint16_t pch,const uint8_t *b,size_t n) {
    if(gpu!=0x0d26 || pch!=0x8c4b || !table(b,n,"DSDT",36) || n!=27679) return false;
    uint64_t hash=14695981039346656037ull;
    for(size_t i=0;i<n;i++) hash=(hash^b[i])*1099511628211ull;
    return hash==0x5c5e79599c550767ull;
}

/* Air6,1 captured DSDT: read-only display admission, no EC or provider gate. */
bool laptop_air61_display_match(uint16_t gpu,uint16_t pch,const uint8_t *b,size_t n) {
    if(gpu!=0x0a26 || pch!=0x9c43 || !table(b,n,"DSDT",36) || n!=31777) return false;
    uint64_t hash=14695981039346656037ull;
    for(size_t i=0;i<n;i++) hash=(hash^b[i])*1099511628211ull;
    return hash==0xaf9a47bc352dc6bfull;
}
