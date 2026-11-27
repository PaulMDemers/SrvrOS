#include <srvros/rtl8188cu_station.h>
#include <srvros/mt76_rsn.h>
#include <srvros/mt76_rx.h>

static const uint8_t rates[]={2,4,11,22,12,18,24,36,48,72,96,108};
static const uint8_t llc[]={0xaa,0xaa,3,0,0,0,0x88,0x8e};
static const uint8_t ssid[]=RTL_CU_STATION_SSID;
static const uint8_t wmm_info[]={221,7,0,0x50,0xf2,2,0,1,0};

static void copy(uint8_t *to,const uint8_t *from,size_t size) {
    for(size_t i=0;i<size;i++)to[i]=from[i];
}

static bool equal(const uint8_t *a,const uint8_t *b,size_t size) {
    for(size_t i=0;i<size;i++)if(a[i]!=b[i])return false;
    return true;
}

static void scrub(void *p,size_t size) {
    volatile uint8_t *out=p;
    while(size--)*out++=0;
}

static unsigned le16(const uint8_t *p) { return p[0]|(unsigned)p[1]<<8; }
static unsigned be16(const uint8_t *p) { return (unsigned)p[0]<<8|p[1]; }
static void put16(uint8_t *p,unsigned v) { p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8); }

static bool unicast(const uint8_t *id) {
    unsigned any=0;
    for(unsigned i=0;i<6;i++)any|=id[i];
    return any && !(id[0]&1);
}

static void tx_descriptor(uint8_t *packet,unsigned length,unsigned sequence,unsigned queue,unsigned pad) {
    put16(packet,length);
    packet[2]=32;
    packet[3]=0x8c;
    packet[4]=0x40; /* Break aggregation. */
    packet[5]=queue;
    packet[6]=7;
    packet[7]=pad?4:0; /* PKT_OFFSET is counted in eight-byte units. */
    put16(packet+14,sequence);
    packet[17]=1;
    packet[21]=0xff;
    packet[22]=1;
    unsigned checksum=0;
    for(unsigned i=0;i<32;i+=2)checksum^=le16(packet+i);
    put16(packet+28,checksum);
}

static bool ethernet_protocol(const uint8_t *protocol) {
    return protocol[0]==8 && (protocol[1]==0 || protocol[1]==6);
}

/* Disjoint, caller-owned Ethernet, CCMP scratch and USB buffers. The
 * software cipher owns the IV and MIC. Use the proven HQ/OUT 2 mapping. */
size_t rtl_cu_data_tx(struct wpa2_ccmp *keys,uint64_t generation,const uint8_t *eth,size_t size,
                       unsigned sequence,uint8_t *plain,uint8_t *usb) {
    static const uint8_t snap[]={0xaa,0xaa,3,0,0,0};
    if(size<14 || size>RTL_CU_ETH_MAX || !sequence || sequence>4095 ||
       !equal(eth+6,keys->peer.spa,6) || !ethernet_protocol(eth+12))return 0;
    uint8_t header[24]={0};
    put16(header,0x0108);
    put16(header+2,314);
    put16(header+22,sequence<<4);
    copy(header+4,keys->peer.aa,6);
    copy(header+10,keys->peer.spa,6);
    copy(header+16,eth,6);
    copy(plain,snap,sizeof snap);
    copy(plain+6,eth+12,size-12);
    size_t wire=32+24+16+size-6;
    unsigned pad=wire%512?0:8;
    scrub(usb,32+pad);
    size_t mpdu=wpa2_ccmp_seal(keys,generation,header,plain,size-6,usb+32+pad,RTL_CU_DATA_TX_MAX-32-pad);
    scrub(plain,size-6);
    if(!mpdu)return 0;
    tx_descriptor(usb,(unsigned)mpdu,sequence,7,pad);
    return mpdu+32+pad;
}

size_t rtl_cu_data_rx(struct wpa2_ccmp *keys,uint64_t generation,const uint8_t *frame,size_t size,
                       uint8_t *plain,uint8_t *eth) {
    static const uint8_t snap[]={0xaa,0xaa,3,0,0,0};
    if(size<48 || !(frame[1]&0x40))return 0;
    size_t n=wpa2_ccmp_open(keys,generation,frame,size,plain,WPA2_CCMP_DATA_MAX);
    bool allowed=n>=8 && n<=RTL_CU_ETH_MAX-6 && equal(plain,snap,6) && ethernet_protocol(plain+6);
    if(allowed) {
        copy(eth,frame+4,6);
        copy(eth+6,frame+16,6);
        copy(eth+12,plain+6,n-6);
    }
    scrub(plain,n);
    return allowed?n+6:0;
}

bool rtl_cu_station_identity(const struct rtl_cu_station_peer *p) {
    return p && unicast(p->station) && unicast(p->bssid) && !equal(p->station,p->bssid,6);
}

static bool supported_rates(const uint8_t *p,size_t size,bool *rate_one) {
    if(!size)return false;
    for(size_t i=0;i<size;i++) {
        unsigned rate=p[i]&127,j=0;
        while(j<sizeof rates && rates[j]!=rate)j++;
        if(j==sizeof rates && (p[i]&128))return false;
        *rate_one|=rate==2;
    }
    return true;
}

bool rtl_cu_station_beacon(struct rtl_cu_station_peer *peer,const uint8_t *mpdu,size_t size) {
    struct mt76_rx_beacon known;
    if(!rtl_cu_station_identity(peer) || peer->rsne_size ||
       wifi_rx_beacon_frame(mpdu,size,RTL_CU_STATION_CHANNEL,&known)!=MT76_RX_OK ||
       !known.ds_present || !known.privacy || !known.rsn_present ||
       known.ssid_size!=sizeof ssid-1 || !equal(known.ssid,ssid,sizeof ssid-1) ||
       !equal(known.bssid,peer->bssid,6) || !equal(known.transmitter,peer->bssid,6))return false;
    const uint8_t *rsne=NULL;
    size_t rsne_size=0;
    bool rate_one=false;
    for(size_t i=36;i<size;) {
        unsigned tag=mpdu[i],n=mpdu[i+1];
        if((tag==1 || tag==50) && !supported_rates(mpdu+i+2,n,&rate_one))return false;
        if(tag==48) {rsne=mpdu+i;rsne_size=n+2;}
        i+=n+2;
    }
    struct mt76_rsn_profile security;
    if(!rate_one || rsne_size>sizeof peer->rsne ||
       mt76_rsn_psk_ccmp(rsne,rsne_size,&security)!=MT76_RSN_OK)return false;
    copy(peer->rsne,rsne,rsne_size);
    peer->rsne_size=rsne_size;
    return true;
}

static bool key_shape(const uint8_t *p,size_t size) {
    return p && size>=99 && size<=1123 && (p[0]==1 || p[0]==2) && p[1]==3 &&
        be16(p+2)==size-4 && p[4]==2 && be16(p+97)==size-99;
}

size_t rtl_cu_station_tx(const struct rtl_cu_station_peer *peer,enum rtl_cu_station_tx type,
                         unsigned sequence,bool qos,const uint8_t *key,size_t key_size,
                         uint8_t *out,size_t capacity) {
    if(!rtl_cu_station_identity(peer) || !peer->rsne_size || !out || !sequence ||
       sequence>4095 || type<RTL_CU_AUTH || type>RTL_CU_LEAVE)return 0;
    bool data=type==RTL_CU_M2 || type==RTL_CU_M4;
    if(data) {
        if(key_size!=(type==RTL_CU_M2?121u:99u) || !key_shape(key,key_size) ||
           be16(key+5)!=(type==RTL_CU_M2?0x010a:0x030a))return 0;
    } else if(key || key_size)return 0;

    uint8_t packet[RTL_CU_STATION_TX_MAX]={0};
    uint8_t *frame=packet+32;
    size_t length=24;
    unsigned fc[]={0xb0,0,0x0108,0x0108,0xc0};
    put16(frame,fc[type]|(data && qos?0x80:0));
    put16(frame+2,314); /* 1 Mbps long preamble ACK plus 2.4 GHz SIFS. */
    copy(frame+4,peer->bssid,6);
    copy(frame+10,peer->station,6);
    copy(frame+16,peer->bssid,6);
    put16(frame+22,sequence<<4);
    if(data) {
        if(qos) {frame[length]=7;length+=2;}
        copy(frame+length,llc,sizeof llc);
        length+=sizeof llc;
        copy(frame+length,key,key_size);
        length+=key_size;
    } else if(type==RTL_CU_AUTH) {
        put16(frame+26,1);
        length+=6;
    } else if(type==RTL_CU_ASSOC) {
        put16(frame+24,0x0431);
        put16(frame+26,10);
        length+=4;
        frame[length++]=0;
        frame[length++]=sizeof ssid-1;
        copy(frame+length,ssid,sizeof ssid-1);
        length+=sizeof ssid-1;
        frame[length++]=1;
        frame[length++]=8;
        copy(frame+length,rates,8);
        length+=8;
        frame[length++]=50;
        frame[length++]=sizeof rates-8;
        copy(frame+length,rates+8,sizeof rates-8);
        length+=sizeof rates-8;
        length+=mt76_rsn_station_ie(frame+length,sizeof packet-32-length);
        copy(frame+length,wmm_info,sizeof wmm_info);
        length+=sizeof wmm_info;
    } else {
        put16(frame+24,3);
        length+=2;
    }
    size_t total=32+length;
    if(total>capacity){scrub(packet,sizeof packet);return 0;}

    /* Observed 32-byte CU USB descriptor. First/last/own, MACID 0,
     * rate table 7, 1 Mbps with the driver rate, manual sequence. */
    tx_descriptor(packet,(unsigned)length,sequence,data?7:0x12,0);
    copy(out,packet,total);
    scrub(packet,sizeof packet);
    return total;
}

enum rtl_cu_station_reply rtl_cu_station_reply(const struct rtl_cu_station_peer *peer,
    enum rtl_cu_station_tx type,const uint8_t *frame,size_t size,struct rtl_cu_station_response *out) {
    if(!out || !rtl_cu_station_identity(peer) || (type!=RTL_CU_AUTH && type!=RTL_CU_ASSOC))
        return RTL_CU_REPLY_INVALID;
    *out=(struct rtl_cu_station_response){0};
    if(!frame || size<24 || (frame[0]&0xfc)!=(type==RTL_CU_AUTH?0xb0:0x10) ||
       !equal(frame+4,peer->station,6) || !equal(frame+10,peer->bssid,6) ||
       !equal(frame+16,peer->bssid,6))return RTL_CU_REPLY_OTHER;
    if(size<30 || (le16(frame)&~0x0800u)!=(type==RTL_CU_AUTH?0xb0:0x10) ||
       (le16(frame+22)&15))return RTL_CU_REPLY_INVALID;
    if(type==RTL_CU_AUTH && (le16(frame+24)!=0 || le16(frame+26)!=2))return RTL_CU_REPLY_INVALID;
    uint16_t status=(uint16_t)le16(frame+(type==RTL_CU_AUTH?28:26));
    bool rate_one=false,base=false,extra=false,qos=false;
    for(size_t i=30;i<size;) {
        if(size-i<2 || frame[i+1]>size-i-2)return RTL_CU_REPLY_INVALID;
        unsigned tag=frame[i],n=frame[i+1];
        const uint8_t *field=frame+i+2;
        if(type==RTL_CU_ASSOC && (tag==1 || tag==50)) {
            bool *seen=tag==1?&base:&extra;
            if(*seen || (tag==1 && n>8) || !supported_rates(field,n,&rate_one))
                return RTL_CU_REPLY_INVALID;
            *seen=true;
        }
        if(tag==221 && n>=7 && equal(field,wmm_info+2,4) && field[4]<=1 && field[5]==1)qos=true;
        i+=n+2;
    }
    out->status=status;
    if(status)return RTL_CU_REPLY_REFUSED;
    if(type==RTL_CU_ASSOC) {
        unsigned aid=le16(frame+28),cap=le16(frame+24);
        if((cap&0x13)!=0x11 || !base || !rate_one || (aid&0xc000)!=0xc000 ||
           !(aid&0x3fff) || (aid&0x3fff)>2007)return RTL_CU_REPLY_INVALID;
        out->aid=(uint16_t)(aid&0x3fff);
        out->qos=qos;
    }
    return RTL_CU_REPLY_OK;
}

bool rtl_cu_station_eapol(const struct rtl_cu_station_peer *peer,const uint8_t *frame,size_t size,
                          const uint8_t **body,size_t *length) {
    if(!body || !length)return false;
    *body=NULL;
    *length=0;
    if(!rtl_cu_station_identity(peer) || !frame || size<24 || (le16(frame+22)&15) ||
       !equal(frame+4,peer->station,6) || !equal(frame+10,peer->bssid,6) ||
       !equal(frame+16,peer->bssid,6))return false;
    unsigned fc=le16(frame)&~0x0800u;
    if(fc!=0x0208 && fc!=0x0288)return false;
    size_t header=fc==0x0288?26:24;
    if(size<header+sizeof llc || (header==26 && frame[24]>7) ||
       !equal(frame+header,llc,sizeof llc))return false;
    header+=sizeof llc;
    if(!key_shape(frame+header,size-header))return false;
    *body=frame+header;
    *length=size-header;
    return true;
}
