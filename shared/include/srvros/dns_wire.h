#ifndef SRVROS_DNS_WIRE_H
#define SRVROS_DNS_WIRE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SRV_DNS_NAME_MAX 255
#define SRV_DNS_PACKET_MAX 1472
#define SRV_DNS_CNAME_MAX 8
struct srv_dns_reader {
    const uint8_t *packet;
    size_t length;
    uint8_t labels[(SRV_DNS_PACKET_MAX + 7) / 8];
};
static inline uint16_t srv_dns_u16(const uint8_t *p) { return (uint16_t)((p[0]<<8)|p[1]); }
static inline uint8_t srv_dns_lower(uint8_t c) { return c>='A' && c<='Z' ? (uint8_t)(c+'a'-'A') : c; }
/* Canonical wire labels preserve label boundaries and embedded bytes. Only
 * backward pointers to already decoded label boundaries may be followed. */
static inline bool srv_dns_name(struct srv_dns_reader *r,size_t *offset,uint8_t out[SRV_DNS_NAME_MAX],size_t *size) {
    size_t pos=*offset,end=0,used=0;
    for(unsigned steps=0;steps<SRV_DNS_PACKET_MAX;steps++) {
        if(pos>=r->length)return false;
        size_t at=pos;uint8_t n=r->packet[pos++];
        if((n&0xc0)==0xc0) {
            if(pos>=r->length)return false;
            size_t target=((size_t)(n&0x3f)<<8)|r->packet[pos++];
            if(target<12 || target>=at || !(r->labels[target/8]&(1u<<(target%8))))return false;
            r->labels[at/8]|=(uint8_t)(1u<<(at%8));
            if(!end)end=pos;
            pos=target;continue;
        }
        if(n&0xc0 || n>r->length-pos || used+1+n>SRV_DNS_NAME_MAX)return false;
        r->labels[at/8]|=(uint8_t)(1u<<(at%8));out[used++]=n;
        if(!n) { *offset=end?end:pos;*size=used;return true; }
        for(unsigned i=0;i<n;i++)out[used++]=srv_dns_lower(r->packet[pos++]);
    }
    return false;
}
static inline bool srv_dns_text(const char *text,uint8_t out[SRV_DNS_NAME_MAX],size_t *size) {
    size_t used=0,start=0;
    for(size_t i=0;i<SRV_DNS_NAME_MAX;i++) {
        if(text[i]!='.' && text[i])continue;
        size_t n=i-start;
        if(!n || n>63 || used+1+n>=SRV_DNS_NAME_MAX)return false;
        out[used++]=(uint8_t)n;
        for(size_t j=start;j<i;j++)out[used++]=srv_dns_lower((uint8_t)text[j]);
        if(!text[i] || !text[i+1]) { out[used++]=0;*size=used;return true; }
        start=i+1;
    }
    return false;
}
static inline bool srv_dns_equal(const uint8_t *a,size_t an,const uint8_t *b,size_t bn) {
    if(an!=bn)return false;
    for(size_t i=0;i<an;i++)if(a[i]!=b[i])return false;
    return true;
}
static inline bool srv_dns_rr(struct srv_dns_reader *r,size_t *offset,uint8_t *owner,size_t *owner_size,
    uint16_t *type,uint16_t *klass,size_t *data,uint16_t *length) {
    if(!srv_dns_name(r,offset,owner,owner_size) || r->length-*offset<10)return false;
    const uint8_t *p=r->packet+*offset;*type=srv_dns_u16(p);*klass=srv_dns_u16(p+2);*length=srv_dns_u16(p+8);
    *offset+=10;*data=*offset;if(*length>r->length-*offset)return false;*offset+=*length;return true;
}
/* -1: malformed/unmatched; 0: matched response without a usable A record;
 * 1: complete validated answer. Output is unchanged unless returning 1. */
static inline int srv_dns_parse_a(const uint8_t *packet,size_t length,uint16_t id,const char *question,uint32_t *ip) {
    if(length<12 || length>SRV_DNS_PACKET_MAX || srv_dns_u16(packet)!=id)return -1;
    uint16_t flags=srv_dns_u16(packet+2),answers=srv_dns_u16(packet+6);
    if(!(flags&0x8000) || (flags&0x7a40) || srv_dns_u16(packet+4)!=1)return -1;
    struct srv_dns_reader r={.packet=packet,.length=length};
    uint8_t current[SRV_DNS_NAME_MAX],owner[SRV_DNS_NAME_MAX],target[SRV_DNS_NAME_MAX];
    size_t current_size,owner_size,offset=12;
    if(!srv_dns_text(question,current,&current_size) || !srv_dns_name(&r,&offset,owner,&owner_size) ||
       !srv_dns_equal(current,current_size,owner,owner_size) || length-offset<4 ||
       srv_dns_u16(packet+offset)!=1 || srv_dns_u16(packet+offset+2)!=1)return -1;
    offset+=4;size_t answer_start=offset;
    uint32_t records=(uint32_t)answers+srv_dns_u16(packet+8)+srv_dns_u16(packet+10);
    for(uint32_t i=0;i<records;i++) {
        uint16_t type,klass,n;size_t data;
        if(!srv_dns_rr(&r,&offset,owner,&owner_size,&type,&klass,&data,&n))return -1;
        if(klass==1 && type==1 && n!=4)return -1;
        if(klass==1 && type==5) {
            size_t end=data,target_size;
            if(!srv_dns_name(&r,&end,target,&target_size) || end!=data+n)return -1;
        }
    }
    if(offset!=length)return -1;
    if(flags&15)return 0;
    for(unsigned hop=0;hop<=SRV_DNS_CNAME_MAX;hop++) {
        bool found_a=false,found_alias=false;uint32_t answer=0;size_t target_size=0;
        offset=answer_start;
        for(unsigned i=0;i<answers;i++) {
            uint16_t type,klass,n;size_t data;
            if(!srv_dns_rr(&r,&offset,owner,&owner_size,&type,&klass,&data,&n))return -1;
            if(klass!=1 || !srv_dns_equal(current,current_size,owner,owner_size))continue;
            if(type==1) {
                if(!found_a)answer=((uint32_t)packet[data]<<24)|((uint32_t)packet[data+1]<<16)|((uint32_t)packet[data+2]<<8)|packet[data+3];
                found_a=true;
            } else if(type==5) {
                size_t end=data,name_size;
                if(!srv_dns_name(&r,&end,owner,&name_size))return -1;
                if(found_alias && !srv_dns_equal(target,target_size,owner,name_size))return -1;
                for(size_t j=0;j<name_size;j++)target[j]=owner[j];
                target_size=name_size;found_alias=true;
            }
        }
        if(found_a && found_alias)return -1;
        if(found_a) { *ip=answer;return 1; }
        if(!found_alias)return 0;
        if(hop==SRV_DNS_CNAME_MAX || srv_dns_equal(current,current_size,target,target_size))return -1;
        for(size_t j=0;j<target_size;j++)current[j]=target[j];
        current_size=target_size;
    }
    return -1;
}
#endif
