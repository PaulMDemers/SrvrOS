#include <srvros/usb_ecm.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint8_t fixture[]={
    9,2,79,0,2,1,0,0x80,50,
    9,4,0,0,1,2,6,0,0,
    5,0x24,0,0x10,1,
    5,0x24,6,0,1,
    13,0x24,15,3,0,0,0,0,0xea,5,0,0,0,
    7,5,0x81,3,16,0,32,
    9,4,1,0,0,10,0,0,0,
    9,4,1,1,2,10,0,0,0,
    7,5,0x82,2,64,0,0,
    7,5,0x02,2,64,0,0
};
static unsigned rng=0xaced1234;
static unsigned random_word(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng; }
static void packet_profiles(void) {
    uint8_t high[sizeof(fixture)];memcpy(high,fixture,sizeof(high));
    for(size_t i=9;i<sizeof(high);i+=high[i]) if(high[i+1]==5) {
        if(high[i+3]==3) high[i+6]=8;else { high[i+4]=0;high[i+5]=2; }
    }
    struct usb_ecm_config c;assert(usb_ecm_parse(high,sizeof(high),3,&c));
    assert(c.in.packet==512 && c.out.packet==512);
    uint8_t super[sizeof(fixture)+18];size_t at=0;
    for(size_t i=0;i<sizeof(fixture);) {
        unsigned n=fixture[i];memcpy(super+at,fixture+i,n);
        bool ep=fixture[i+1]==5;
        if(ep) { if(fixture[i+3]==3)super[at+6]=8;else {super[at+4]=0;super[at+5]=4;} }
        at+=n;
        if(ep) { uint8_t companion[]={6,0x30,0,0,0,0};if(fixture[i+3]==3)companion[4]=16;else companion[2]=15;memcpy(super+at,companion,6);at+=6; }
        i+=n;
    }
    assert(at==sizeof(super));super[2]=sizeof(super);
    assert(usb_ecm_parse(super,sizeof(super),4,&c));
    assert(c.in.packet==1024 && c.in.burst==15 && c.out.burst==15);
    assert(usb_ecm_parse(super,sizeof(super),5,&c));
    /* A bulk stream capability is not supported by the transfer-ring driver. */
    super[sizeof(super)-3]=1;assert(!usb_ecm_parse(super,sizeof(super),4,&c));
}
static void malformed_inputs(void) {
    /* Exact-sized allocations make the same run useful under ASan/UBSan. */
    for(unsigned round=0;round<20000;round++) {
        size_t length=round<4096?round:sizeof(fixture);
        uint8_t *bytes=malloc(length?length:1);assert(bytes);
        for(size_t i=0;i<length;i++)bytes[i]=(uint8_t)random_word();
        if(length>=sizeof(fixture))memcpy(bytes,fixture,sizeof(fixture));
        if(length>=4) { bytes[2]=(uint8_t)length;bytes[3]=(uint8_t)(length>>8); }
        for(unsigned i=0;i<3 && length;i++)bytes[random_word()%length]=(uint8_t)random_word();
        struct usb_ecm_config c,before;memset(&c,0xa7,sizeof(c));before=c;
        bool ok=usb_ecm_parse(bytes,length,1+random_word()%5,&c);
        if(!ok)assert(!memcmp(&c,&before,sizeof(c)));
        else assert(c.in.address&128 && !(c.out.address&128) && c.notify.address&128 && c.segment>=1514);
        uint8_t mac[6]={1,2,3,4,5,6},old[6];memcpy(old,mac,6);
        if(!usb_ecm_mac(bytes,length,mac))assert(!memcmp(mac,old,6));
        struct usb_ecm_link link={0},saved=link;
        if(!usb_ecm_notification(&link,bytes,length,0))assert(!memcmp(&link,&saved,sizeof(link)));
        free(bytes);
    }
}
static void captured_asix(void) {
    uint8_t bytes[]={0x09,0x02,0x62,0x00,0x02,0x03,0x00,0xa0,0x17,0x09,0x04,0x00,0x00,0x01,0x02,0x06,0x00,0x00,0x05,0x24,0x00,0x10,0x01,0x05,0x24,0x06,0x00,0x01,0x0d,0x24,0x0f,0x05,0x00,0x00,0x00,0x00,0xea,0x05,0x40,0x80,0x00,0x07,0x05,0x81,0x03,0x10,0x00,0x0b,0x06,0x30,0x00,0x00,0x00,0x00,0x09,0x04,0x01,0x00,0x00,0x0a,0x00,0x00,0x00,0x09,0x04,0x01,0x01,0x02,0x0a,0x00,0x00,0x00,0x07,0x05,0x82,0x02,0x00,0x04,0x00,0x06,0x30,0x05,0x00,0x00,0x00,0x07,0x05,0x03,0x02,0x00,0x04,0x00,0x06,0x30,0x02,0x00,0x00,0x00};
    struct usb_ecm_config c;
    assert(!usb_ecm_parse(bytes,sizeof(bytes),4,&c));
    assert(usb_ecm_parse_device(bytes,sizeof(bytes),4,0x0b95,0x1790,0x0200,&c));
    assert(c.configuration==3 && c.control_interface==0 && c.data_interface==1 && c.data_alt==1 && c.mac_string==5);
    assert(c.notify_data_interface);
    assert(c.notify.packet==16 && c.in.packet==1024 && c.in.burst==5 && c.out.address==3 && c.out.burst==2);
    assert(!usb_ecm_parse_device(bytes,sizeof(bytes),4,0x0b95,0x1790,0x0100,&c));
    assert(!usb_ecm_parse_device(bytes,sizeof(bytes),4,0x0b95,0x1791,0x0200,&c));
    assert(!usb_ecm_parse_device(bytes,sizeof(bytes),4,0x0b96,0x1790,0x0200,&c));
    assert(!usb_ecm_parse_device(bytes,sizeof(bytes),3,0x0b95,0x1790,0x0200,&c));
    for(unsigned i=0;i<sizeof(bytes);i++) {
        bytes[i]^=0x80;
        if(!usb_ecm_parse(bytes,sizeof(bytes),4,&c)) assert(!usb_ecm_parse_device(bytes,sizeof(bytes),4,0x0b95,0x1790,0x0200,&c));
        bytes[i]^=0x80;
    }
    /* A generic-valid sibling descriptor must not inherit this firmware quirk. */
    bytes[52]=16;
    assert(usb_ecm_parse_device(bytes,sizeof(bytes),4,0x0b95,0x1790,0x0200,&c));
    assert(!c.notify_data_interface);bytes[52]=0;
    for(unsigned n=0;n<sizeof(bytes);n++) assert(!usb_ecm_parse_device(bytes,n,4,0x0b95,0x1790,0x0200,&c));
}
int main(void) {
    captured_asix();
    fixture[2]=sizeof(fixture);struct usb_ecm_config c;
    assert(usb_ecm_parse(fixture,sizeof(fixture),1,&c));
    assert(!c.notify_data_interface);
    assert(c.control_interface==0&&c.data_interface==1&&c.data_alt==1&&c.mac_string==3&&c.segment==1514);
    assert(c.in.address==0x82&&c.out.address==2&&c.notify.address==0x81);
    for(size_t n=0;n<sizeof(fixture);n++) assert(!usb_ecm_parse(fixture,n,1,&c));
    assert(!usb_ecm_parse(fixture,sizeof(fixture),2,&c));assert(!usb_ecm_parse(fixture,sizeof(fixture),3,&c));
    uint8_t bad[sizeof(fixture)];memcpy(bad,fixture,sizeof(bad));bad[27]=0;assert(!usb_ecm_parse(bad,sizeof(bad),1,&c));
    memcpy(bad,fixture,sizeof(bad));bad[18]=0;assert(!usb_ecm_parse(bad,sizeof(bad),1,&c));
    memcpy(bad,fixture,sizeof(bad));bad[30]=0;assert(!usb_ecm_parse(bad,sizeof(bad),1,&c));
    memcpy(bad,fixture,sizeof(bad));bad[sizeof(bad)-5]=0x82;assert(!usb_ecm_parse(bad,sizeof(bad),1,&c));
    uint8_t macdesc[26]={26,3},mac[6];const char *text="40010203aBcd";
    for(unsigned i=0;i<12;i++) macdesc[2+i*2]=text[i];
    assert(usb_ecm_mac(macdesc,sizeof(macdesc),mac));assert(mac[0]==0x40&&mac[4]==0xab&&mac[5]==0xcd);
    macdesc[4]='1';assert(!usb_ecm_mac(macdesc,26,mac));macdesc[4]='0';macdesc[3]=1;assert(!usb_ecm_mac(macdesc,26,mac));
    struct usb_ecm_link link={0};uint8_t note[]={0xa1,0,1,0,0,0,0,0};
    assert(usb_ecm_notification(&link,note,8,0)&&link.known&&link.up);
    note[4]=1;assert(!usb_ecm_notification(&link,note,8,0));note[4]=0;note[2]=0;
    assert(usb_ecm_notification(&link,note,8,0)&&!link.up);
    uint8_t speed[]={0xa1,0x2a,0,0,0,0,8,0,0,0xe1,0xf5,5,0,0xe1,0xf5,5};
    assert(usb_ecm_notification(&link,speed,16,0)&&link.upstream_bps==100000000&&link.downstream_bps==100000000);
    assert(!usb_ecm_notification(&link,speed,15,0));
    packet_profiles();malformed_inputs();
    puts("usb-ecm: PASS interface/union/endpoints, malformed/truncated descriptors, full/high/super-speed endpoints, 20,000 malformed inputs, MAC and link notifications");
}
