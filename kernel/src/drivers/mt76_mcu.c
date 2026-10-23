#include <srvros/mt76_mcu.h>

/* Wire fields follow Linux v6.18 mt76x02_dma.h and
 * mt76x02u_skb_dma_info / __mt76x02u_mcu_send_msg. */
size_t mt76_mcu_encode(uint8_t command, uint8_t sequence,
    const uint8_t *payload, size_t length, uint8_t *out, size_t capacity) {
    if (!out || !payload || !length || length > MT76_MCU_PAYLOAD_MAX ||
        !command || command > 127 || !sequence || sequence > 15) return 0;
    size_t padded = (length + 3u) & ~(size_t)3u;
    size_t total = padded + 8u;
    if (capacity < total) return 0;
    uint32_t header = (uint32_t)padded | ((uint32_t)sequence << 16) |
        ((uint32_t)command << 20) | (2u << 27) | (1u << 30);
    for (size_t i = 0; i < 4; ++i) out[i] = (uint8_t)(header >> (8u * i));
    for (size_t i = 0; i < length; ++i) out[4 + i] = payload[i];
    for (size_t i = 4 + length; i < total; ++i) out[i] = 0;
    return total;
}

size_t mt76_mcu_select_response_queue(uint8_t *out, size_t capacity) {
    /* CMD_FUN_SET_OP=1, Q_SELECT=1, queue=1. Keep generic seq0 refused. */
    static const uint8_t packet[16]={8,0,0x10,0x50,1,0,0,0,1,0,0,0,0,0,0,0};
    if(!out || capacity<sizeof packet)return 0;
    for(size_t i=0;i<sizeof packet;i++)out[i]=packet[i];
    return sizeof packet;
}

size_t mt76_mcu_register_packet(bool write, uint8_t sequence,
    const struct mt76_mcu_register_pair *pairs, size_t count,
    uint8_t *out, size_t capacity) {
    if(!pairs || !out || !count || count>MT76_MCU_REGISTER_PAIRS_MAX ||
        !sequence || sequence>15 || capacity<count*8u+8u)return 0;
    for(size_t i=0;i<count;i++)
        if(pairs[i].address>0x3ffcu || (pairs[i].address&3u) ||
            (!write && pairs[i].value))return 0;
    uint8_t payload[MT76_MCU_PAYLOAD_MAX];
    for(size_t i=0;i<count;i++) {
        uint32_t address=0x410000u+pairs[i].address;
        for(size_t b=0;b<4;b++) {
            payload[8*i+b]=(uint8_t)(address>>(8*b));
            payload[8*i+4+b]=(uint8_t)(pairs[i].value>>(8*b));
        }
    }
    return mt76_mcu_encode(write?12:10,sequence,payload,count*8u,out,capacity);
}

bool mt76_mcu_decode(const uint8_t *data, size_t length,
    struct mt76_mcu_response *out) {
    if (!data || !out || length < 8 || length > MT76_MCU_RESPONSE_MAX) return false;
    uint32_t header = 0;
    for (size_t i = 0; i < 4; ++i) header |= (uint32_t)data[i] << (8u * i);
    *out = (struct mt76_mcu_response){
        .sequence = (uint8_t)((header >> 16) & 15u),
        .event = (uint8_t)((header >> 20) & 15u),
        .reported_length = (uint16_t)(header & 0x3fffu)
    };
    return true;
}

bool mt76_mcu_validate_identity_reply(const uint8_t *data, size_t length) {
    /* Native session00065e74311ea511-00, firmware pinned by the caller.
     * Header includes seq1,event0,length8 and captured port/type fields.
     * Trailer repeats the header; accepting only a matching seq is insufficient. */
    static const uint8_t expected[16]={
        0x08,0x00,0x01,0x4a, 0x00,0x10,0x41,0x00,
        0x00,0x20,0x50,0x76, 0x08,0x00,0x01,0x4a
    };
    if(!data || length!=sizeof expected)return false;
    for(size_t i=0;i<sizeof expected;i++)if(data[i]!=expected[i])return false;
    return true;
}

bool mt76_mcu_validate_disabled_write_reply(const uint8_t *data, size_t length) {
    /* Native session00065e74d8311f56-00; fixed seq2/address411004/value0.
     * Do not generalize this observed echo contract to other commands. */
    static const uint8_t expected[16]={
        0x08,0x00,0x02,0x4a, 0x04,0x10,0x41,0x00,
        0x00,0x00,0x00,0x00, 0x08,0x00,0x02,0x4a
    };
    if(!data || length!=sizeof expected)return false;
    for(size_t i=0;i<sizeof expected;i++)if(data[i]!=expected[i])return false;
    return true;
}

bool mt76_mcu_validate_timing_reply(const uint8_t *data, size_t length) {
    /* Native session00065e753e794b20-00: first entry echoed; both values
     * independently read back. ACK alone cannot accept the second write. */
    static const uint8_t expected[16]={
        8,0,3,0x4a, 0,0x11,0x41,0, 0x10,0x10,0xa4,0x33, 8,0,3,0x4a
    };
    if(!data || length!=sizeof expected)return false;
    for(size_t i=0;i<sizeof expected;i++)if(data[i]!=expected[i])return false;
    return true;
}
static enum mt76_mcu_timing_status timing_read(struct mt76_mcu_timing *s,
    const struct mt76_mcu_timing_io *io,uint32_t address,uint32_t *value) {
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->reads++;
    if(!io->read(io->context,address,value))return MT76_TIMING_IO;
    return *value==UINT32_MAX?MT76_TIMING_STATE:MT76_TIMING_OK;
}
enum mt76_mcu_timing_status mt76_mcu_apply_timing(struct mt76_mcu_timing *s,
    const struct mt76_mcu_timing_io *io) {
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange)
        return MT76_TIMING_REFUSED;
    *s=(struct mt76_mcu_timing){.attempted=true,.received=-1};
    enum mt76_mcu_timing_status status;
#define TIMING_READ(address,value) do { status=timing_read(s,io,address,value); if(status!=MT76_TIMING_OK)return status; } while(0)
    TIMING_READ(0x1004,&s->mac_before);
    if(s->mac_before)return MT76_TIMING_STATE;
    TIMING_READ(0x1100,&s->before[0]);
    TIMING_READ(0x1104,&s->before[1]);
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    const struct mt76_mcu_register_pair pairs[2]={{0x1100,0x33a41010},{0x1104,0x209}};
    uint8_t packet[24],reply[MT76_MCU_RESPONSE_MAX];
    size_t length=mt76_mcu_register_packet(true,3,pairs,2,packet,sizeof packet);
    if(length!=sizeof packet)return MT76_TIMING_STATE;
    s->write_attempted=true;
    s->received=io->exchange(io->context,packet,length,reply,sizeof reply);
    if(s->received<0)return MT76_TIMING_IO;
    s->ack_validated=mt76_mcu_validate_timing_reply(reply,(size_t)s->received);
    if(!s->ack_validated)return MT76_TIMING_REPLY;
    TIMING_READ(0x1100,&s->after[0]);
    TIMING_READ(0x1104,&s->after[1]);
    TIMING_READ(0x1004,&s->mac_after);
    if(s->after[0]!=pairs[0].value || s->after[1]!=pairs[1].value || s->mac_after)
        return MT76_TIMING_STATE;
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
#undef TIMING_READ
}

/* Numeric register settings from Linux v6.18 mt76x02_regs.h and mt76x0
 * initialization: use final model HT rate setting, not the common intermediate.
 * These four configuration registers do not enable MAC transmit/receive. */
static const struct mt76_mcu_register_pair mac_basics[4]={
    {0x1408,0x13f},{0x140c,0x4003},{0x1400,0x17f97},{0x1018,0xa1fff}
};
/* Frame representation only: disable hardware offloads before any data path.
 * Numeric settings from Linux v6.18 MT7610 model register definitions. */
static const struct mt76_mcu_register_pair mac_format[4]={
    {0x0250,0},{0x0260,0},{0x1410,0x1ff},{0x150c,3}
};
/* Initial BBP CORE configuration, numeric Linux v6.18 model settings only.
 * Guarded by the exact native CORE0 identity, not a whole imported table. */
static const struct mt76_mcu_register_pair bbp_core[4]={
    {0x2004,2},{0x2010,0},{0x2060,0},{0x2080,0x4003000a}
};
/* Remaining CORE and initial IBI/AGC settings; MAC stays disabled. */
static const struct mt76_mcu_register_pair bbp_front[4]={
    {0x20a8,0},{0x20b0,0},{0x212c,0x0fde8081},{0x2300,0x00021400}
};
/* First AGC settings after CORE/IBI; numeric Linux v6.18 MT7610 settings. */
static const struct mt76_mcu_register_pair bbp_agc[4]={
    {0x2304,3},{0x2308,0x003a6464},{0x233c,0x88a28cb8},{0x2358,0x1e21}
};
/* Same accepted values, changed targets last to verify the full larger packet. */
static const struct mt76_mcu_register_pair bbp_batch[4]={
    {0x233c,0x88a28cb8},{0x2358,0x1e21},{0x2304,3},{0x2308,0x003a6464}
};
/* Next fixed AGC initialization quartet; numeric Linux v6.18 settings. */
static const struct mt76_mcu_register_pair agc_more[4]={
    {0x235c,0x272c},{0x2360,0x2f3a},{0x2364,0x8000005a},{0x2368,0x007c2005}
};
static const struct mt76_mcu_register_pair agc_next[4]={
    {0x2384,0x3238},{0x2388,0x000a0c0c},{0x2394,0x2121262c},{0x23a4,0x38383e45}
};
static const struct mt76_mcu_register_pair agc_tail[4]={
    {0x23e4,0x1010},{0x23ec,0xbaa20e96},{0x23fc,1},{0x2400,0x00280403}
};
static const struct mt76_mcu_register_pair rxc_start[4]={
    {0x2404,0},{0x2504,0x12},{0x2508,0x11},{0x250c,5}
};
static const struct mt76_mcu_register_pair rxc_tail[4]={
    {0x2510,0},{0x2514,0xf977c4ec},{0x251c,0x90},{0x2620,0}
};
static const struct mt76_mcu_register_pair txbe_start[4]={
    {0x2700,0},{0x2710,4},{0x2718,0},{0x2720,0x14}
};
static const struct mt76_mcu_register_pair txbe_next[4]={
    {0x2724,0x20000000},{0x2728,0},{0x2730,0},{0x2734,0}
};
_Static_assert(MT76_MCU_COMPACT_PHASES<=12,"compact phases must not wrap command identifiers");
static const struct mt76_mcu_register_pair *compact_plan(unsigned phase) {
    switch(phase) {
    case 0:return mac_basics;case 1:return mac_format;case 2:return bbp_core;
    case 3:return bbp_front;case 4:return bbp_batch;case 5:return agc_more;case 6:return agc_next;case 7:return agc_tail;case 8:return rxc_start;case 9:return rxc_tail;case 10:return txbe_start;case 11:return txbe_next;default:return 0;
    }
}
bool mt76_mcu_compact_read_allowed(unsigned phase,uint32_t address) {
    const struct mt76_mcu_register_pair *plan=compact_plan(phase);
    if(!plan)return false;
    if(address==0x1004 || (phase>=2 && address==0x2000))return true;
    for(unsigned n=0;n<4;n++)if(address==plan[n].address)return true;
    return false;
}
static size_t fixed_mac_packet(const struct mt76_mcu_register_pair *plan,
    unsigned sequence,unsigned batch,unsigned pairs,uint8_t *out,size_t capacity) {
    if((pairs!=2 && pairs!=4) || batch>=4/pairs)return 0;
    return mt76_mcu_register_packet(true,(uint8_t)(sequence+batch),plan+pairs*batch,pairs,out,capacity);
}
static bool fixed_mac_reply(const struct mt76_mcu_register_pair *plan,
    unsigned sequence,unsigned batch,unsigned pairs,const uint8_t *data,size_t length) {
    if((pairs!=2 && pairs!=4) || batch>=4/pairs || !data || length!=16)return false;
    uint32_t header=0x4a000008u|((sequence+batch)<<16);
    const uint32_t words[4]={header,0x410000u+plan[pairs*batch].address,plan[pairs*batch].value,header};
    for(unsigned i=0;i<16;i++)if(data[i]!=(uint8_t)(words[i/4]>>(8*(i%4))))return false;
    return true;
}
size_t mt76_mcu_mac_packet(unsigned batch,uint8_t *out,size_t capacity) {
    return fixed_mac_packet(mac_basics,4,batch,2,out,capacity);
}
bool mt76_mcu_validate_mac_reply(unsigned batch,const uint8_t *data,size_t length) {
    return fixed_mac_reply(mac_basics,4,batch,2,data,length);
}
size_t mt76_mcu_format_packet(unsigned batch,uint8_t *out,size_t capacity) {
    return fixed_mac_packet(mac_format,6,batch,2,out,capacity);
}
bool mt76_mcu_validate_format_reply(unsigned batch,const uint8_t *data,size_t length) {
    return fixed_mac_reply(mac_format,6,batch,2,data,length);
}
size_t mt76_mcu_bbp_packet(unsigned batch,uint8_t *out,size_t capacity) {
    return fixed_mac_packet(bbp_core,8,batch,2,out,capacity);
}
bool mt76_mcu_validate_bbp_reply(unsigned batch,const uint8_t *data,size_t length) {
    return fixed_mac_reply(bbp_core,8,batch,2,data,length);
}
size_t mt76_mcu_bbp_front_packet(unsigned batch,uint8_t *out,size_t capacity) {
    return fixed_mac_packet(bbp_front,10,batch,2,out,capacity);
}
bool mt76_mcu_validate_bbp_front_reply(unsigned batch,const uint8_t *data,size_t length) {
    return fixed_mac_reply(bbp_front,10,batch,2,data,length);
}
size_t mt76_mcu_bbp_agc_packet(unsigned batch,uint8_t *out,size_t capacity) {
    return fixed_mac_packet(bbp_agc,12,batch,2,out,capacity);
}
bool mt76_mcu_validate_bbp_agc_reply(unsigned batch,const uint8_t *data,size_t length) {
    return fixed_mac_reply(bbp_agc,12,batch,2,data,length);
}
size_t mt76_mcu_bbp_batch_packet(unsigned batch,uint8_t *out,size_t capacity) {
    return fixed_mac_packet(bbp_batch,14,batch,4,out,capacity);
}
bool mt76_mcu_validate_bbp_batch_reply(unsigned batch,const uint8_t *data,size_t length) {
    return fixed_mac_reply(bbp_batch,14,batch,4,data,length);
}
size_t mt76_mcu_compact_packet(unsigned phase,uint8_t *out,size_t capacity) {
    const struct mt76_mcu_register_pair *plan=compact_plan(phase);
    return plan?fixed_mac_packet(plan,4+phase,0,4,out,capacity):0;
}
bool mt76_mcu_validate_compact_reply(unsigned phase,const uint8_t *data,size_t length) {
    const struct mt76_mcu_register_pair *plan=compact_plan(phase);
    return plan && fixed_mac_reply(plan,4+phase,0,4,data,length);
}
static enum mt76_mcu_timing_status mac_read(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io,uint32_t address,uint32_t *value) {
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->reads++;
    if(!io->read(io->context,address,value))return MT76_TIMING_IO;
    return *value==UINT32_MAX?MT76_TIMING_STATE:MT76_TIMING_OK;
}
static enum mt76_mcu_timing_status apply_fixed_mac(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io,const struct mt76_mcu_register_pair *plan,unsigned sequence,bool guard_core,unsigned pairs) {
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange ||
        (pairs!=2 && pairs!=4))return MT76_TIMING_REFUSED;
    *s=(struct mt76_mcu_mac){.attempted=true,.received=-1};
    enum mt76_mcu_timing_status status;
#define MAC_READ(address,value) do { status=mac_read(s,io,address,value); if(status!=MT76_TIMING_OK)return status; } while(0)
    for(unsigned batch=0;batch<4/pairs;batch++) {
        MAC_READ(0x1004,&s->mac_before);
        if(s->mac_before)return MT76_TIMING_STATE;
        if(guard_core) {
            MAC_READ(0x2000,&s->core_before);
            if(s->core_before!=0xf000f200)return MT76_TIMING_STATE;
        }
        for(unsigned n=0;n<pairs;n++) {
            unsigned i=pairs*batch+n;
            MAC_READ(plan[i].address,&s->before[i]);
        }
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        uint8_t packet[40],reply[MT76_MCU_RESPONSE_MAX];
        size_t length=fixed_mac_packet(plan,sequence,batch,pairs,packet,sizeof packet);
        if(length!=pairs*8u+8u)return MT76_TIMING_STATE;
        s->batches_attempted++;
        s->received=io->exchange(io->context,packet,length,reply,sizeof reply);
        if(s->received<0)return MT76_TIMING_IO;
        if(!fixed_mac_reply(plan,sequence,batch,pairs,reply,(size_t)s->received))return MT76_TIMING_REPLY;
        s->ack_mask|=1u<<batch;
        for(unsigned n=0;n<pairs;n++) {
            unsigned i=pairs*batch+n;
            MAC_READ(plan[i].address,&s->after[i]);
            if(s->after[i]!=plan[i].value)return MT76_TIMING_STATE;
        }
        MAC_READ(0x1004,&s->mac_after);
        if(s->mac_after)return MT76_TIMING_STATE;
        if(guard_core) {
            MAC_READ(0x2000,&s->core_after);
            if(s->core_after!=0xf000f200)return MT76_TIMING_STATE;
        }
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        s->batches_completed++;
    }
    s->complete=true;return MT76_TIMING_OK;
#undef MAC_READ
}

enum mt76_mcu_timing_status mt76_mcu_apply_mac_basics(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) { return apply_fixed_mac(s,io,mac_basics,4,false,2); }
enum mt76_mcu_timing_status mt76_mcu_apply_mac_format(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) { return apply_fixed_mac(s,io,mac_format,6,false,2); }

enum mt76_mcu_timing_status mt76_mcu_apply_bbp_core(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) { return apply_fixed_mac(s,io,bbp_core,8,true,2); }

enum mt76_mcu_timing_status mt76_mcu_apply_bbp_front(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) { return apply_fixed_mac(s,io,bbp_front,10,true,2); }

enum mt76_mcu_timing_status mt76_mcu_apply_bbp_agc(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) { return apply_fixed_mac(s,io,bbp_agc,12,true,2); }

enum mt76_mcu_timing_status mt76_mcu_apply_bbp_batch(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) { return apply_fixed_mac(s,io,bbp_batch,14,true,4); }

enum mt76_mcu_timing_status mt76_mcu_apply_compact(unsigned phase,
    struct mt76_mcu_mac *s,const struct mt76_mcu_timing_io *io) {
    const struct mt76_mcu_register_pair *plan=compact_plan(phase);
    return plan?apply_fixed_mac(s,io,plan,4+phase,phase>=2,4):MT76_TIMING_REFUSED;
}

bool mt76_mcu_begin(struct mt76_mcu_session *s, uint8_t *sequence) {
    if (!s || !sequence || s->poisoned || s->pending || s->issued >= 15) return false;
    s->pending = ++s->issued;
    *sequence = s->pending;
    return true;
}

bool mt76_mcu_retire(struct mt76_mcu_session *s, uint8_t sequence,
    bool validated_completion) {
    if (!s || s->poisoned || !s->pending) return false;
    if (!validated_completion || sequence != s->pending) {
        s->poisoned = true;
        return false;
    }
    s->pending = 0;
    return true;
}
