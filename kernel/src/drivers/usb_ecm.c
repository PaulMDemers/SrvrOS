#include <srvros/usb_ecm.h>
static unsigned word(const uint8_t *p) { return p[0]|(unsigned)p[1]<<8; }
static uint32_t dword(const uint8_t *p) { return word(p)|(uint32_t)word(p+2)<<16; }
static bool endpoint(const uint8_t *b,size_t n,size_t off,unsigned speed,bool interrupt,struct usb_ecm_endpoint *out) {
    const uint8_t *p=b+off;
    if(p[0]<7 || !(p[2]&15) || (p[2]&0x70) || p[3]!=(interrupt?3:2)) return false;
    unsigned packet=word(p+4);if(!packet || (packet&0xf800)) return false;
    if(interrupt) { if(!(p[2]&128)||packet<16||packet>64||!p[6] || (speed>=3 && p[6]>16)) return false; }
    else if((speed==1 && packet!=8 && packet!=16 && packet!=32 && packet!=64) ||
            (speed==3 && packet!=512) || (speed>=4 && packet!=1024)) return false;
    *out=(struct usb_ecm_endpoint){.address=p[2],.packet=packet,.interval=p[6]};
    if(speed>=4) {
        size_t c=off+p[0];if(n-c<6||b[c]!=6||b[c+1]!=0x30||b[c+2]>15||b[c+3]) return false;
        unsigned payload=word(b+c+4);
        if(interrupt) { if(b[c+2] || payload!=packet) return false; }
        else if(payload) return false;
        out->burst=b[c+2];
    }
    return true;
}
bool usb_ecm_parse(const uint8_t *b,size_t n,unsigned speed,struct usb_ecm_config *out) {
    if(!b||!out||n<9||n>4096||b[0]!=9||b[1]!=2||word(b+2)!=n||!b[5]||b[4]<2||speed==2||speed<1||speed>5) return false;
    struct usb_ecm_config c={.configuration=b[5]};
    bool control=false,have_control=false,have_union=false,have_ether=false;
    for(size_t off=9;off<n;) {
        if(n-off<2 || b[off]<2 || b[off]>n-off) return false;
        const uint8_t *p=b+off;
        if(p[1]==4) {
            if(p[0]<9) return false;
            control=!p[3]&&p[5]==2&&p[6]==6&&p[7]==0;
            if(control) { if(have_control||p[4]!=1) return false;have_control=true;c.control_interface=p[2]; }
        } else if(control && p[1]==0x24) {
            if(p[0]<3) return false;
            if(p[2]==6) {
                if(have_union||p[0]!=5||p[3]!=c.control_interface||p[4]==p[3]) return false;
                have_union=true;c.data_interface=p[4];
            } else if(p[2]==15) {
                if(have_ether||p[0]!=13||!p[3]) return false;
                have_ether=true;c.mac_string=p[3];c.segment=word(p+8);
                if(c.segment<1514||c.segment>4096) return false;
            }
        } else if(control && p[1]==5) {
            if(c.notify.address||!endpoint(b,n,off,speed,true,&c.notify)) return false;
        }
        off+=p[0];
    }
    if(!have_control||!have_union||!have_ether||!c.notify.address) return false;
    bool data=false,have_data=false,zero_alt=false;unsigned count=0;
    for(size_t off=9;off<n;) {
        const uint8_t *p=b+off;
        if(p[1]==4) {
            if(data && count!=2) return false;
            data=false;
            if(p[2]==c.data_interface) {
                if(p[5]!=10||p[6]||p[7]) return false;
                if(!p[3]) { if(p[4] || zero_alt) return false;zero_alt=true; }
                else if(p[4]==2) { if(have_data) return false;have_data=true;data=true;count=0;c.data_alt=p[3]; }
                else return false;
            }
        } else if(data && p[1]==5) {
            struct usb_ecm_endpoint e;
            if(!endpoint(b,n,off,speed,false,&e)) return false;
            struct usb_ecm_endpoint *target=(e.address&128)?&c.in:&c.out;
            if(target->address||e.address==c.notify.address) return false;
            *target=e;count++;
        }
        off+=p[0];
    }
    if(!zero_alt||!have_data||!c.in.address||!c.out.address||(data&&count!=2)) return false;
    *out=c;return true;
}
static int hex(unsigned char c) { if(c>='0'&&c<='9') return c-'0';if(c>='a'&&c<='f') return c-'a'+10;if(c>='A'&&c<='F') return c-'A'+10;return -1; }
bool usb_ecm_mac(const uint8_t *p,size_t n,uint8_t mac[6]) {
    if(!p||!mac||n<26||p[0]!=26||p[1]!=3) return false;
    uint8_t parsed[6];unsigned any=0;
    for(unsigned i=0;i<6;i++) {
        int a=hex(p[2+i*4]),b=hex(p[4+i*4]);
        if(a<0||b<0||p[3+i*4]||p[5+i*4]) return false;
        parsed[i]=(uint8_t)(a*16+b);any|=parsed[i];
    }
    if(!any || (parsed[0]&1)) return false;
    for(unsigned i=0;i<6;i++) mac[i]=parsed[i];
    return true;
}
bool usb_ecm_notification(struct usb_ecm_link *link,const uint8_t *p,size_t n,uint8_t interface) {
    if(!link||!p||n<8||p[0]!=0xa1||word(p+4)!=interface||word(p+6)!=n-8) return false;
    unsigned value=word(p+2);
    if(p[1]==0 && n==8 && value<=1) {
        link->known=true;link->up=value!=0;
        if(!value) link->upstream_bps=link->downstream_bps=0;
        return true;
    }
    if(p[1]==0x2a && n==16 && !value) {
        link->downstream_bps=dword(p+8);link->upstream_bps=dword(p+12);return true;
    }
    return false;
}

/* 0b95:1790 rev 0200, captured ECM configuration 3. Its interrupt
 * companion reports zero wBytesPerInterval for a 16-byte endpoint. Normalize
 * only this exact descriptor in a private parser copy; do not alter firmware,
 * bulk capacities or descriptors from sibling revisions/configurations. */
bool usb_ecm_parse_device(const uint8_t *b,size_t n,unsigned speed,uint16_t vendor,uint16_t product,uint16_t revision,struct usb_ecm_config *out) {
    if(usb_ecm_parse(b,n,speed,out)) return true;
    static const uint8_t captured[]={0x09,0x02,0x62,0x00,0x02,0x03,0x00,0xa0,0x17,0x09,0x04,0x00,0x00,0x01,0x02,0x06,0x00,0x00,0x05,0x24,0x00,0x10,0x01,0x05,0x24,0x06,0x00,0x01,0x0d,0x24,0x0f,0x05,0x00,0x00,0x00,0x00,0xea,0x05,0x40,0x80,0x00,0x07,0x05,0x81,0x03,0x10,0x00,0x0b,0x06,0x30,0x00,0x00,0x00,0x00,0x09,0x04,0x01,0x00,0x00,0x0a,0x00,0x00,0x00,0x09,0x04,0x01,0x01,0x02,0x0a,0x00,0x00,0x00,0x07,0x05,0x82,0x02,0x00,0x04,0x00,0x06,0x30,0x05,0x00,0x00,0x00,0x07,0x05,0x03,0x02,0x00,0x04,0x00,0x06,0x30,0x02,0x00,0x00,0x00};
    if(!b || !out || vendor!=0x0b95 || product!=0x1790 || revision!=0x0200 || speed!=4 || n!=sizeof(captured)) return false;
    uint8_t normalized[sizeof(captured)];
    for(unsigned i=0;i<sizeof(captured);i++) { if(b[i]!=captured[i]) return false;normalized[i]=b[i]; }
    normalized[52]=16;
    return usb_ecm_parse(normalized,sizeof(normalized),speed,out);
}
