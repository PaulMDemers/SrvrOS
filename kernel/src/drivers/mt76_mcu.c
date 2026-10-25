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
    const struct mt76_mcu_timing_io *io,const struct mt76_mcu_register_pair *plan,unsigned sequence,bool guard_core,unsigned pairs,unsigned total) {
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange ||
        (pairs!=2 && pairs!=4) || (total!=2 && total!=4) || pairs>total)return MT76_TIMING_REFUSED;
    *s=(struct mt76_mcu_mac){.attempted=true,.received=-1};
    enum mt76_mcu_timing_status status;
#define MAC_READ(address,value) do { status=mac_read(s,io,address,value); if(status!=MT76_TIMING_OK)return status; } while(0)
    for(unsigned batch=0;batch<total/pairs;batch++) {
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
    const struct mt76_mcu_timing_io *io) { return apply_fixed_mac(s,io,mac_basics,4,false,2,4); }
enum mt76_mcu_timing_status mt76_mcu_apply_mac_format(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) { return apply_fixed_mac(s,io,mac_format,6,false,2,4); }

enum mt76_mcu_timing_status mt76_mcu_apply_bbp_core(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) { return apply_fixed_mac(s,io,bbp_core,8,true,2,4); }

enum mt76_mcu_timing_status mt76_mcu_apply_bbp_front(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) { return apply_fixed_mac(s,io,bbp_front,10,true,2,4); }

enum mt76_mcu_timing_status mt76_mcu_apply_bbp_agc(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) { return apply_fixed_mac(s,io,bbp_agc,12,true,2,4); }

enum mt76_mcu_timing_status mt76_mcu_apply_bbp_batch(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) { return apply_fixed_mac(s,io,bbp_batch,14,true,4,4); }

enum mt76_mcu_timing_status mt76_mcu_apply_compact(unsigned phase,
    struct mt76_mcu_mac *s,const struct mt76_mcu_timing_io *io) {
    const struct mt76_mcu_register_pair *plan=compact_plan(phase);
    return plan?apply_fixed_mac(s,io,plan,4+phase,phase>=2,4,4):MT76_TIMING_REFUSED;
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

static const uint32_t txbe_observe_addresses[8]={
    0x1004,0x2000,0x2724,0x2728,0x2730,0x2734,0x1004,0x2000
};
bool mt76_txbe_read_allowed(uint32_t address) {
    for(unsigned i=0;i<8;i++)if(address==txbe_observe_addresses[i])return true;
    return false;
}
enum mt76_mcu_timing_status mt76_txbe_observe(struct mt76_txbe_observation *s,
    const struct mt76_txbe_io *io) {
    if(!s || s->attempted || !io || !io->authorize || !io->read)return MT76_TIMING_REFUSED;
    s->attempted=true;
    for(unsigned n=0;n<MT76_TXBE_SAMPLES;n++) {
        for(unsigned i=0;i<8;i++) {
            if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
            uint32_t v=0;
            if(!io->read(io->context,txbe_observe_addresses[i],&v))return MT76_TIMING_IO;
            s->reads++;s->values[n][i]=v;
            if(v==UINT32_MAX || ((i==0 || i==6) && v!=0) ||
                ((i==1 || i==7) && v!=0xf000f200))return MT76_TIMING_STATE;
        }
        s->samples++;
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

size_t mt76_txbe_first_packet(uint8_t *out,size_t capacity) {
    const struct mt76_mcu_register_pair pair={0x2724,0x20000000};
    return mt76_mcu_register_packet(true,15,&pair,1,out,capacity);
}
enum mt76_mcu_timing_status mt76_txbe_first_apply(struct mt76_txbe_first *s,
    const struct mt76_mcu_timing_io *io) {
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange)return MT76_TIMING_REFUSED;
    s->attempted=true;
    const struct mt76_txbe_io read_io={io->context,io->authorize,io->read};
    enum mt76_mcu_timing_status status=mt76_txbe_observe(&s->before,&read_io);
    if(status!=MT76_TIMING_OK)return status;
    uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX];
    if(mt76_txbe_first_packet(packet,sizeof packet)!=sizeof packet)return MT76_TIMING_REFUSED;
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->write_attempted=true;
    s->received=io->exchange(io->context,packet,sizeof packet,reply,sizeof reply);
    if(s->received<0)return MT76_TIMING_IO;
    /* Same first address/value/sequence echo as phase11, not its four writes. */
    if(!mt76_mcu_validate_compact_reply(11,reply,(size_t)s->received))return MT76_TIMING_REPLY;
    s->ack_validated=true;
    status=mt76_txbe_observe(&s->after,&read_io);
    if(status!=MT76_TIMING_OK)return status;
    for(unsigned n=0;n<MT76_TXBE_SAMPLES;n++)if(s->after.values[n][2]!=0x20000000)return MT76_TIMING_STATE;
    s->complete=true;return MT76_TIMING_OK;
}

/* Isolate TXBE10 with TXBE9 held at its captured zero baseline. */
size_t mt76_txbe_second_packet(uint8_t *out,size_t capacity) {
    const struct mt76_mcu_register_pair pair={0x2728,0};
    return mt76_mcu_register_packet(true,15,&pair,1,out,capacity);
}
enum mt76_mcu_timing_status mt76_txbe_second_apply(struct mt76_txbe_first *s,
    const struct mt76_mcu_timing_io *io) {
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange)return MT76_TIMING_REFUSED;
    s->attempted=true;
    const struct mt76_txbe_io read_io={io->context,io->authorize,io->read};
    enum mt76_mcu_timing_status status=mt76_txbe_observe(&s->before,&read_io);
    if(status!=MT76_TIMING_OK)return status;
    for(unsigned n=0;n<MT76_TXBE_SAMPLES;n++)if(s->before.values[n][2]!=0)return MT76_TIMING_STATE;
    uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX];
    if(mt76_txbe_second_packet(packet,sizeof packet)!=sizeof packet)return MT76_TIMING_REFUSED;
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->write_attempted=true;
    s->received=io->exchange(io->context,packet,sizeof packet,reply,sizeof reply);
    if(s->received<0)return MT76_TIMING_IO;
    const uint32_t words[4]={0x4a0f0008,0x412728,0,0x4a0f0008};
    if(s->received!=16)return MT76_TIMING_REPLY;
    for(unsigned i=0;i<16;i++)if(reply[i]!=(uint8_t)(words[i/4]>>(8*(i%4))))return MT76_TIMING_REPLY;
    s->ack_validated=true;
    status=mt76_txbe_observe(&s->after,&read_io);
    if(status!=MT76_TIMING_OK)return status;
    for(unsigned n=0;n<MT76_TXBE_SAMPLES;n++)if((s->after.values[n][2]!=0 || s->after.values[n][3]!=0))return MT76_TIMING_STATE;
    s->complete=true;return MT76_TIMING_OK;
}

size_t mt76_txbe_access_packet(uint8_t *out,size_t capacity) {
    const struct mt76_mcu_register_pair pair={0x2728,0};
    return mt76_mcu_register_packet(false,15,&pair,1,out,capacity);
}
enum mt76_mcu_timing_status mt76_txbe_access_apply(struct mt76_txbe_access *s,
    const struct mt76_mcu_timing_io *io) {
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange)return MT76_TIMING_REFUSED;
    s->attempted=true;
    const struct mt76_txbe_io read_io={io->context,io->authorize,io->read};
    enum mt76_mcu_timing_status status=mt76_txbe_observe(&s->before,&read_io);
    if(status!=MT76_TIMING_OK)return status;
    for(unsigned n=0;n<MT76_TXBE_SAMPLES;n++)if(s->before.values[n][2]!=0)return MT76_TIMING_STATE;
    uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX];
    if(mt76_txbe_access_packet(packet,sizeof packet)!=sizeof packet)return MT76_TIMING_REFUSED;
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->request_attempted=true;
    s->received=io->exchange(io->context,packet,sizeof packet,reply,sizeof reply);
    if(s->received<0)return MT76_TIMING_IO;
    if(s->received!=16)return MT76_TIMING_REPLY;
    const uint32_t words[4]={0x4a0f0008,0x412728,0,0x4a0f0008};
    for(unsigned i=0;i<16;i++)if((i<8 || i>=12) &&
        reply[i]!=(uint8_t)(words[i/4]>>(8*(i%4))))return MT76_TIMING_REPLY;
    s->value=0;
    for(unsigned i=0;i<4;i++)s->value|=(uint32_t)reply[8+i]<<(8*i);
    if(s->value==UINT32_MAX)return MT76_TIMING_STATE;
    s->reply_validated=true;
    status=mt76_txbe_observe(&s->after,&read_io);
    if(status!=MT76_TIMING_OK)return status;
    for(unsigned n=0;n<MT76_TXBE_SAMPLES;n++) {
        if(s->before.values[n][3]==s->value)s->matches++;
        if(s->after.values[n][3]==s->value)s->matches++;
    }
    for(unsigned n=0;n<MT76_TXBE_SAMPLES;n++)if(s->after.values[n][2]!=0)return MT76_TIMING_STATE;
    if(s->matches!=2*MT76_TXBE_SAMPLES)return MT76_TIMING_STATE;
    s->complete=true;return MT76_TIMING_OK;
}

/* Isolated EP0 zero write; MCU reply and all subsequent EP0 samples must be zero. */
enum mt76_mcu_timing_status mt76_txbe_direct_apply(struct mt76_txbe_access *s,
    const struct mt76_txbe_direct_io *direct) {
    if(!direct || !direct->write_zero)return MT76_TIMING_REFUSED;
    const struct mt76_mcu_timing_io *io=&direct->access;
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange)return MT76_TIMING_REFUSED;
    s->attempted=true;
    const struct mt76_txbe_io read_io={io->context,io->authorize,io->read};
    enum mt76_mcu_timing_status status=mt76_txbe_observe(&s->before,&read_io);
    if(status!=MT76_TIMING_OK)return status;
    for(unsigned n=0;n<MT76_TXBE_SAMPLES;n++)if(s->before.values[n][2]!=0)return MT76_TIMING_STATE;
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->control_attempted=true;
    if(!direct->write_zero(io->context))return MT76_TIMING_IO;
    s->control_written=true;
    uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX];
    if(mt76_txbe_access_packet(packet,sizeof packet)!=sizeof packet)return MT76_TIMING_REFUSED;
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->request_attempted=true;
    s->received=io->exchange(io->context,packet,sizeof packet,reply,sizeof reply);
    if(s->received<0)return MT76_TIMING_IO;
    if(s->received!=16)return MT76_TIMING_REPLY;
    const uint32_t words[4]={0x4a0f0008,0x412728,0,0x4a0f0008};
    for(unsigned i=0;i<16;i++)if((i<8 || i>=12) &&
        reply[i]!=(uint8_t)(words[i/4]>>(8*(i%4))))return MT76_TIMING_REPLY;
    s->value=0;
    for(unsigned i=0;i<4;i++)s->value|=(uint32_t)reply[8+i]<<(8*i);
    if(s->value==UINT32_MAX)return MT76_TIMING_STATE;
    s->reply_validated=true;
    status=mt76_txbe_observe(&s->after,&read_io);
    if(status!=MT76_TIMING_OK)return status;
    for(unsigned n=0;n<MT76_TXBE_SAMPLES;n++) {
        if(s->after.values[n][3]==s->value)s->matches++;
    }
    for(unsigned n=0;n<MT76_TXBE_SAMPLES;n++)if(s->after.values[n][2]!=0)return MT76_TIMING_STATE;
    if(s->matches!=MT76_TXBE_SAMPLES || s->value!=0)return MT76_TIMING_STATE;
    s->complete=true;return MT76_TIMING_OK;
}

/* One ordered pair at the final unused identifier; no writes to TXBE12/13. */
size_t mt76_mcu_clock_pair_packet(uint8_t *out,size_t capacity) {
    return fixed_mac_packet(txbe_next,15,0,2,out,capacity);
}
enum mt76_mcu_timing_status mt76_mcu_apply_clock_pair(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) {
    return apply_fixed_mac(s,io,txbe_next,15,true,2,2);
}

/* Linux v7.0 numeric BBP initialization values; explicit observation only. */
static const struct mt76_mcu_register_pair full_bbp[MT76_BBP_TABLE_ENTRIES]={
    {0x2004,0x2},
    {0x2010,0x0},
    {0x2060,0x0},
    {0x2080,0x4003000a},
    {0x20a8,0x0},
    {0x20b0,0x0},
    {0x212c,0xfde8081},
    {0x2300,0x21400},
    {0x2304,0x3},
    {0x2308,0x3a6464},
    {0x233c,0x88a28cb8},
    {0x2358,0x1e21},
    {0x235c,0x272c},
    {0x2360,0x2f3a},
    {0x2364,0x8000005a},
    {0x2368,0x7c2005},
    {0x2384,0x3238},
    {0x2388,0xa0c0c},
    {0x2394,0x2121262c},
    {0x23a4,0x38383e45},
    {0x23e4,0x1010},
    {0x23ec,0xbaa20e96},
    {0x23fc,0x1},
    {0x2400,0x280403},
    {0x2404,0x0},
    {0x2504,0x12},
    {0x2508,0x11},
    {0x250c,0x5},
    {0x2510,0x0},
    {0x2514,0xf977c4ec},
    {0x251c,0x90},
    {0x2620,0x0},
    {0x2700,0x0},
    {0x2710,0x4},
    {0x2718,0x0},
    {0x2720,0x14},
    {0x2724,0x20000000},
    {0x2728,0x0},
    {0x2730,0x0},
    {0x2734,0x0},
    {0x2738,0x0},
    {0x273c,0x0},
    {0x2740,0x0},
    {0x2744,0x0},
    {0x2804,0x8800},
    {0x280c,0x0},
    {0x2810,0x0},
    {0x2934,0x192},
    {0x2938,0x60612},
    {0x293c,0xc8321b18},
    {0x2940,0x1e},
    {0x2944,0x0},
    {0x2948,0xcc00a993},
    {0x294c,0xb9cb9cb9},
    {0x2950,0x26c00057},
    {0x2954,0x1},
    {0x2960,0x6},
    {0x2970,0x3f},
};
uint32_t mt76_bbp_table_address(unsigned i){return i<MT76_BBP_TABLE_ENTRIES?full_bbp[i].address:UINT32_MAX;}
uint32_t mt76_bbp_table_value(unsigned i){return i<MT76_BBP_TABLE_ENTRIES?full_bbp[i].value:UINT32_MAX;}
bool mt76_bbp_table_read_allowed(uint32_t address){
    if(address==0x1004 || address==0x2000)return true;
    for(unsigned i=0;i<MT76_BBP_TABLE_ENTRIES;i++)if(full_bbp[i].address==address)return true;
    return false;
}
size_t mt76_bbp_table_packet(unsigned chunk,uint8_t *out,size_t capacity){
    if(chunk>=3 || !out)return 0;
    unsigned start=24*chunk,count=chunk==2?10:24,bytes=count*8+8;
    if(capacity<bytes)return 0;
    uint32_t header=0x50c00000u|((chunk==2?6u:0u)<<16)|(count*8);
    for(unsigned i=0;i<bytes;i++)out[i]=0;
    for(unsigned i=0;i<4;i++)out[i]=(uint8_t)(header>>(8*i));
    for(unsigned n=0;n<count;n++)for(unsigned i=0;i<4;i++){
        out[4+8*n+i]=(uint8_t)((0x410000+full_bbp[start+n].address)>>(8*i));
        out[8+8*n+i]=(uint8_t)(full_bbp[start+n].value>>(8*i));
    }
    return bytes;
}
static enum mt76_mcu_timing_status table_read(struct mt76_bbp_table *s,
    const struct mt76_mcu_timing_io *io,uint32_t address,uint32_t *value){
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    if(!io->read(io->context,address,value))return MT76_TIMING_IO;
    s->reads++;return *value==UINT32_MAX?MT76_TIMING_STATE:MT76_TIMING_OK;
}
enum mt76_mcu_timing_status mt76_bbp_table_capture(struct mt76_bbp_table *s,
    const struct mt76_mcu_timing_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange)return MT76_TIMING_REFUSED;
    s->attempted=true;enum mt76_mcu_timing_status status;
#define TABLE_READ(a,v) do{status=table_read(s,io,a,v);if(status!=MT76_TIMING_OK)return status;}while(0)
    for(unsigned stage=0;stage<2;stage++){
        TABLE_READ(0x1004,&s->mac[stage*2]);if(s->mac[stage*2])return MT76_TIMING_STATE;
        TABLE_READ(0x2000,&s->core[stage*2]);if(s->core[stage*2]!=0xf000f200)return MT76_TIMING_STATE;
        for(unsigned i=0;i<MT76_BBP_TABLE_ENTRIES;i++)TABLE_READ(full_bbp[i].address,stage?&s->after[i]:&s->before[i]);
        TABLE_READ(0x1004,&s->mac[stage*2+1]);if(s->mac[stage*2+1])return MT76_TIMING_STATE;
        TABLE_READ(0x2000,&s->core[stage*2+1]);if(s->core[stage*2+1]!=0xf000f200)return MT76_TIMING_STATE;
        if(!stage)for(unsigned chunk=0;chunk<3;chunk++){
            if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
            uint8_t packet[200],reply[MT76_MCU_RESPONSE_MAX];size_t length=mt76_bbp_table_packet(chunk,packet,sizeof packet);
            s->chunks_attempted++;int received=io->exchange(io->context,packet,length,reply,sizeof reply);
            if(received<0)return MT76_TIMING_IO;
            if(chunk<2){if(received!=0)return MT76_TIMING_REPLY;}
            else{
                if(received!=16)return MT76_TIMING_REPLY;
                const uint32_t words[]={0x4a060008,0x412938,0x60612,0x4a060008};
                for(unsigned i=0;i<16;i++)if(reply[i]!=(uint8_t)(words[i/4]>>(8*(i%4))))return MT76_TIMING_REPLY;
                s->ack_validated=true;
            }
            s->chunks_completed++;
        }
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    for(unsigned i=0;i<MT76_BBP_TABLE_ENTRIES;i++)if(s->after[i]!=full_bbp[i].value)s->mismatches++;
    s->complete=true;return MT76_TIMING_OK;
#undef TABLE_READ
}

/* Fixed read-only inventory of reference MAC prerequisites; no write admission. */
static const uint32_t mac_snapshot_addresses[MT76_MAC_SNAPSHOT_ENTRIES]={
    0x041c,
    0x0420,
    0x1408,
    0x140c,
    0x1004,
    0x1400,
    0x1104,
    0x1330,
    0x1334,
    0x1350,
    0x1348,
    0x1018,
    0x102c,
    0x0408,
    0x040c,
    0x134c,
    0x1404,
    0x1364,
    0x1368,
    0x0404,
    0x0208,
    0x1374,
    0x1378,
    0x136c,
    0x1370,
    0x1340,
    0x1344,
    0x1380,
    0x1608,
    0x1100,
    0x1204,
    0x0124,
    0x0400,
    0x0800,
    0x1030,
    0x1338,
    0x0a44,
    0x0260,
    0x0250,
    0x1218,
    0x1220,
    0x13b0,
    0x13c0,
    0x1314,
    0x1318,
    0x131c,
    0x1320,
    0x1324,
    0x13d4,
    0x13d8,
    0x13dc,
    0x150c,
    0x1238,
    0x006c,
    0x0070,
    0x1410,
    0x13e0,
    0x13e4,
    0x13e8,
    0x1358,
    0x141c,
    0x080c,
    0x0230,
};
uint32_t mt76_mac_snapshot_address(unsigned index){return index<MT76_MAC_SNAPSHOT_ENTRIES?mac_snapshot_addresses[index]:UINT32_MAX;}
bool mt76_mac_snapshot_read_allowed(uint32_t address){
    if(address==0x2000)return true;
    for(unsigned i=0;i<MT76_MAC_SNAPSHOT_ENTRIES;i++)if(address==mac_snapshot_addresses[i])return true;
    return false;
}
enum mt76_mcu_timing_status mt76_mac_snapshot_capture(struct mt76_mac_snapshot *s,
    const struct mt76_mcu_timing_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->read)return MT76_TIMING_REFUSED;
    s->attempted=true;
    for(unsigned n=0;n<MT76_MAC_SNAPSHOT_ENTRIES+4;n++){
        unsigned edge=n<2?0:1;
        uint32_t address,*value;
        if(n==0 || n==MT76_MAC_SNAPSHOT_ENTRIES+2){address=0x1004;value=&s->mac[edge];}
        else if(n==1 || n==MT76_MAC_SNAPSHOT_ENTRIES+3){address=0x2000;value=&s->core[edge];}
        else{address=mac_snapshot_addresses[n-2];value=&s->values[n-2];}
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        if(!io->read(io->context,address,value))return MT76_TIMING_IO;
        s->reads++;
        if(*value==UINT32_MAX || (address==0x1004 && *value) || (address==0x2000 && *value!=0xf000f200))return MT76_TIMING_STATE;
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

/* Observational only: all-ones target data never authorizes subsequent writes. */
enum mt76_mcu_timing_status mt76_mac_raw_capture(struct mt76_mac_snapshot *s,
    const struct mt76_mcu_timing_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->read)return MT76_TIMING_REFUSED;
    s->attempted=true;
    for(unsigned n=0;n<70;n++){
        uint32_t address,*value;
        if(n==0 || n==67){address=0x1000;value=&s->asic[n==67];}
        else if(n==1 || n==68){address=0x1004;value=&s->mac[n==68];}
        else if(n==2 || n==69){address=0x2000;value=&s->core[n==69];}
        else if(n==66){address=0x1030;value=&s->repeat_1030;}
        else{address=mac_snapshot_addresses[n-3];value=&s->values[n-3];}
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        if(!io->read(io->context,address,value))return MT76_TIMING_IO;
        s->reads++;
        if((address==0x1000 && *value!=0x76502000) ||
           (address==0x1004 && *value) || (address==0x2000 && *value!=0xf000f200))return MT76_TIMING_STATE;
        if(*value==UINT32_MAX)s->all_ones++;
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

/* Captured 080c=03ff0233,0230=41040: clear bit4, then replace low10 bits with201. */
static const struct mt76_mcu_register_pair mac_route_plan[2]={{0x080c,0x03ff0223},{0x0230,0x00041201}};
size_t mt76_mac_route_packet(uint8_t *out,size_t capacity){
    return mt76_mcu_register_packet(true,6,mac_route_plan,2,out,capacity);
}
enum mt76_mcu_timing_status mt76_mac_route_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io){return apply_fixed_mac(s,io,mac_route_plan,6,true,2,2);}

/* Linux v7.0 common_mac_reg_table order; bounded diagnostic, MAC remains off. */
static const struct mt76_mcu_register_pair mac_policy_plan[4]={
    {0x1350,0x1020},{0x1348,0xa2090},{0x134c,0x47d01f0f},{0x1404,0x13}
};
size_t mt76_mac_policy_packet(uint8_t *out,size_t capacity){
    return mt76_mcu_register_packet(true,7,mac_policy_plan,4,out,capacity);
}
enum mt76_mcu_timing_status mt76_mac_policy_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io){return apply_fixed_mac(s,io,mac_policy_plan,7,true,4,4);}

/* Observations only: completion does not accept target values. */
enum mt76_mcu_timing_status mt76_mac_policy_observe(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io,uint32_t *mismatch_mask) {
    if(!mismatch_mask)return MT76_TIMING_REFUSED;
    const struct mt76_mcu_register_pair *plan=mac_policy_plan;
    const unsigned sequence=7,pairs=4,total=4;const bool guard_core=true;
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange ||
        (pairs!=2 && pairs!=4) || (total!=2 && total!=4) || pairs>total)return MT76_TIMING_REFUSED;
    *mismatch_mask=0;
    *s=(struct mt76_mcu_mac){.attempted=true,.received=-1};
    enum mt76_mcu_timing_status status;
#define MAC_READ(address,value) do { status=mac_read(s,io,address,value); if(status!=MT76_TIMING_OK)return status; } while(0)
    for(unsigned batch=0;batch<total/pairs;batch++) {
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
            if(s->after[i]!=plan[i].value)*mismatch_mask|=1u<<i;
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

/* Exact readback observed on captured 7392:a711; do not generalize to a mask. */
enum mt76_mcu_timing_status mt76_mac_policy_validate(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io,uint32_t *mismatch_mask) {
    enum mt76_mcu_timing_status status=mt76_mac_policy_observe(s,io,mismatch_mask);
    if(status!=MT76_TIMING_OK)return status;
    const uint32_t expected[4]={0x007f1020,0x000a2090,0x47d01f0f,0x00000013};
    for(unsigned i=0;i<4;i++)if(s->after[i]!=expected[i]){
        s->complete=false;return MT76_TIMING_STATE;
    }
    return MT76_TIMING_OK;
}

/* Linux v7.0 common table relative order; MAC remains disabled. */
static const struct mt76_mcu_register_pair mac_protect_plan[4]={
    {0x1364,0x07f40003},{0x1368,0x07f42004},{0x1374,0x01742004},{0x1378,0x03f42084}
};
size_t mt76_mac_protect_packet(uint8_t *out,size_t capacity){
    return mt76_mcu_register_packet(true,8,mac_protect_plan,4,out,capacity);
}
enum mt76_mcu_timing_status mt76_mac_protect_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io){return apply_fixed_mac(s,io,mac_protect_plan,8,true,4,4);}

/* Remaining common-table mixed-mode protection and TXOP/RTS, MAC disabled. */
static const struct mt76_mcu_register_pair mac_contention_plan[4]={
    {0x136c,0x01742004},{0x1370,0x03f42084},{0x1340,0x0000583f},{0x1344,0x00ffff20}
};
size_t mt76_mac_contention_packet(uint8_t *out,size_t capacity){
    return mt76_mcu_register_packet(true,9,mac_contention_plan,4,out,capacity);
}
enum mt76_mcu_timing_status mt76_mac_contention_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io){return apply_fixed_mac(s,io,mac_contention_plan,9,true,4,4);}

/* mt76x0 final TX software controls, header translation disabled; MAC stays off. */
static const struct mt76_mcu_register_pair mac_txsw_plan[4]={
    {0x1330,0x00000601},{0x1334,0x00040000},{0x1338,0},{0x0260,0}
};
size_t mt76_mac_txsw_packet(uint8_t *out,size_t capacity){
    return mt76_mcu_register_packet(true,10,mac_txsw_plan,4,out,capacity);
}
enum mt76_mcu_timing_status mt76_mac_txsw_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io){return apply_fixed_mac(s,io,mac_txsw_plan,10,true,4,4);}

/* Common-table beacon SRAM offsets only; no beacon upload or MAC enable. */
static const struct mt76_mcu_register_pair mac_beacon_plan[2]={
    {0x041c,0xf8f0e8e0},{0x0420,0x6f77d0c8}
};
size_t mt76_mac_beacon_packet(uint8_t *out,size_t capacity){
    return mt76_mcu_register_packet(true,11,mac_beacon_plan,2,out,capacity);
}
enum mt76_mcu_timing_status mt76_mac_beacon_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io){return apply_fixed_mac(s,io,mac_beacon_plan,11,true,2,2);}

/* Final mt76x0 VHT protection and fallback table values; MAC remains off. */
static const struct mt76_mcu_register_pair mac_vht_plan[4]={
    {0x13e0,0xe3f42004},{0x13e4,0xe3f42084},{0x13e8,0xe3f42104},{0x1358,0xedcba980}
};
size_t mt76_mac_vht_packet(uint8_t *out,size_t capacity){
    return mt76_mcu_register_packet(true,12,mac_vht_plan,4,out,capacity);
}
enum mt76_mcu_timing_status mt76_mac_vht_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io){return apply_fixed_mac(s,io,mac_vht_plan,12,true,4,4);}

/* Packet-buffer limits and final configuration; segmentation offload stays off. */
static const struct mt76_mcu_register_pair mac_buffer_plan[4]={
    {0x0408,0x1fbf1f1f},{0x040c,0x0000fe9f},{0x0404,0x77723c1f},{0x0250,0}
};
size_t mt76_mac_buffer_packet(uint8_t *out,size_t capacity){
    return mt76_mcu_register_packet(true,13,mac_buffer_plan,4,out,capacity);
}
enum mt76_mcu_timing_status mt76_mac_buffer_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io){return apply_fixed_mac(s,io,mac_buffer_plan,13,true,4,4);}

/* Observation of FCE control, with the previously accepted L2 routing retained. */
static const struct mt76_mcu_register_pair mac_fce_plan[2]={
    {0x0800,1},{0x080c,0x03ff0223}
};
size_t mt76_mac_fce_packet(uint8_t *out,size_t capacity){
    return mt76_mcu_register_packet(true,14,mac_fce_plan,2,out,capacity);
}
/* Completion records FCE control evidence; it does not accept that target. */
enum mt76_mcu_timing_status mt76_mac_fce_observe(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io,uint32_t *mismatch_mask) {
    if(!mismatch_mask)return MT76_TIMING_REFUSED;
    const struct mt76_mcu_register_pair *plan=mac_fce_plan;
    const unsigned sequence=14,pairs=2,total=2;const bool guard_core=true;
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange ||
        (pairs!=2 && pairs!=4) || (total!=2 && total!=4) || pairs>total)return MT76_TIMING_REFUSED;
    *mismatch_mask=0;
    *s=(struct mt76_mcu_mac){.attempted=true,.received=-1};
    enum mt76_mcu_timing_status status;
#define MAC_READ(address,value) do { status=mac_read(s,io,address,value); if(status!=MT76_TIMING_OK)return status; } while(0)
    for(unsigned batch=0;batch<total/pairs;batch++) {
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
            if(s->after[i]!=plan[i].value){
                *mismatch_mask|=1u<<i;
                if(i!=0)return MT76_TIMING_STATE;
            }
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

/* Exact write1/read5 profile captured on 7392:a711; no writable-bit mask inferred. */
enum mt76_mcu_timing_status mt76_mac_fce_validate(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io,uint32_t *mismatch_mask) {
    enum mt76_mcu_timing_status status=mt76_mac_fce_observe(s,io,mismatch_mask);
    if(status!=MT76_TIMING_OK)return status;
    if(s->after[0]!=5 || s->after[1]!=0x03ff0223 || *mismatch_mask!=1){
        s->complete=false;return MT76_TIMING_STATE;
    }
    return MT76_TIMING_OK;
}

/* Aggregation length limit, retaining the accepted frame-engine L2 routing. */
static const struct mt76_mcu_register_pair mac_ampdu_plan[2]={
    {0x1030,0xaaa99887},{0x080c,0x03ff0223}
};
size_t mt76_mac_ampdu_packet(uint8_t *out,size_t capacity){
    return mt76_mcu_register_packet(true,15,mac_ampdu_plan,2,out,capacity);
}
enum mt76_mcu_timing_status mt76_mac_ampdu_apply(struct mt76_mcu_mac *s,
    const struct mt76_mcu_timing_io *io) {
    const struct mt76_mcu_register_pair *plan=mac_ampdu_plan;
    const unsigned sequence=15,pairs=2,total=2;const bool guard_core=true;
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange ||
        (pairs!=2 && pairs!=4) || (total!=2 && total!=4) || pairs>total)return MT76_TIMING_REFUSED;
    *s=(struct mt76_mcu_mac){.attempted=true,.received=-1};
    enum mt76_mcu_timing_status status;
#define MAC_READ(address,value) do { status=mac_read(s,io,address,value); if(status!=MT76_TIMING_OK)return status; } while(0)
    for(unsigned batch=0;batch<total/pairs;batch++) {
        MAC_READ(0x1004,&s->mac_before);
        if(s->mac_before)return MT76_TIMING_STATE;
        if(guard_core) {
            MAC_READ(0x2000,&s->core_before);
            if(s->core_before!=0xf000f200)return MT76_TIMING_STATE;
        }
        for(unsigned n=0;n<pairs;n++) {
            unsigned i=pairs*batch+n;
            if(i==0) {
                /* Captured raw all-ones observation: only this initial1030 read.
                 * Transport must succeed; all post-write reads remain strict. */
                if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
                s->reads++;
                if(!io->read(io->context,0x1030,&s->before[0]))return MT76_TIMING_IO;
                if(s->before[0]!=UINT32_MAX)return MT76_TIMING_STATE;
            } else {
                MAC_READ(plan[i].address,&s->before[i]);
                if(s->before[i]!=plan[i].value)return MT76_TIMING_STATE;
            }
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

/* Fixed Linux v7.0 G-band/20MHz defaults; no RF/channel or MAC enable. */
static const struct mt76_mcu_register_pair bbp_switch[20]={
    {0x2310,0x1feda049},
    {0x2318,0x45},
    {0x2320,0x16344ef0},
    {0x2330,0x5052879},
    {0x2334,0x35050004},
    {0x2338,0x310f2e3c},
    {0x2368,0x7c2005},
    {0x236c,0xe1},
    {0x2370,0x60806},
    {0x237c,0xe23},
    {0x2380,0x3218},
    {0x2384,0x3240},
    {0x238c,0x11111616},
    {0x239c,0x2a2a3036},
    {0x23ac,0x27273438},
    {0x23cc,0x17171c1c},
    {0x23d4,0x26262a2f},
    {0x23dc,0x40404040},
    {0x23e8,0x1010},
    {0x2800,0x3d5000e0},
};
uint32_t mt76_bbp_switch_address(unsigned i){return i<20?bbp_switch[i].address:UINT32_MAX;}
uint32_t mt76_bbp_switch_value(unsigned i){return i<20?bbp_switch[i].value:UINT32_MAX;}
bool mt76_bbp_switch_read_allowed(uint32_t a){
    if(a==0x1004 || a==0x2000)return true;
    for(unsigned i=0;i<20;i++)if(a==bbp_switch[i].address)return true;
    return false;
}
static enum mt76_mcu_timing_status switch_read(struct mt76_bbp_switch *s,const struct mt76_bbp_switch_io *io,uint32_t a,uint32_t *v){
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    if(!io->read(io->context,a,v))return MT76_TIMING_IO;
    s->reads++;return *v==UINT32_MAX?MT76_TIMING_STATE:MT76_TIMING_OK;
}
enum mt76_mcu_timing_status mt76_bbp_switch_apply(struct mt76_bbp_switch *s,const struct mt76_bbp_switch_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->write)return MT76_TIMING_REFUSED;
    s->attempted=true;enum mt76_mcu_timing_status status;
#define SWITCH_READ(a,v) do{status=switch_read(s,io,a,v);if(status!=MT76_TIMING_OK)return status;}while(0)
#define SWITCH_GUARD(m,c) do{SWITCH_READ(0x1004,m);if(*(m))return MT76_TIMING_STATE;SWITCH_READ(0x2000,c);if(*(c)!=0xf000f200)return MT76_TIMING_STATE;}while(0)
    for(unsigned stage=0;stage<2;stage++){
        SWITCH_GUARD(&s->mac[stage*2],&s->core[stage*2]);
        for(unsigned i=0;i<20;i++)SWITCH_READ(bbp_switch[i].address,stage?&s->after[i]:&s->before[i]);
        SWITCH_GUARD(&s->mac[stage*2+1],&s->core[stage*2+1]);
        if(!stage)for(unsigned i=0;i<20;i++){
            SWITCH_GUARD(&s->write_mac[i],&s->write_core[i]);
            if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
            s->writes_attempted++;
            if(!io->write(io->context,i,bbp_switch[i].address,bbp_switch[i].value))return MT76_TIMING_IO;
            s->writes_completed++;
        }
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    for(unsigned i=0;i<20;i++)if(s->after[i]!=bbp_switch[i].value)s->mismatches++;
    s->complete=true;return MT76_TIMING_OK;
#undef SWITCH_READ
#undef SWITCH_GUARD
}

/* Reference DCOC defaults, not a calibration command or RF enable. */
static const struct mt76_mcu_register_pair dcoc_plan[9]={
    {0x2cbc,0x10f0},
    {0x2cc0,0x8080},
    {0x2cc4,0xf07},
    {0x2cc8,0x40},
    {0x2ccc,0x404},
    {0x2cd0,0x80803},
    {0x2cd4,0x704},
    {0x2cd8,0x2828},
    {0x2cdc,0x5050},
};
uint32_t mt76_dcoc_address(unsigned i){return i<9?dcoc_plan[i].address:UINT32_MAX;}
uint32_t mt76_dcoc_value(unsigned i){return i<9?dcoc_plan[i].value:UINT32_MAX;}
bool mt76_dcoc_read_allowed(uint32_t a){
    if(a==0x1004 || a==0x2000)return true;
    for(unsigned i=0;i<9;i++)if(a==dcoc_plan[i].address)return true;
    return false;
}
size_t mt76_dcoc_packet(uint8_t *out,size_t capacity){return mt76_mcu_register_packet(true,7,dcoc_plan,9,out,capacity);}
static enum mt76_mcu_timing_status dcoc_read(struct mt76_dcoc *s,const struct mt76_mcu_timing_io *io,uint32_t a,uint32_t *v){
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    if(!io->read(io->context,a,v))return MT76_TIMING_IO;
    s->reads++;return *v==UINT32_MAX?MT76_TIMING_STATE:MT76_TIMING_OK;
}
enum mt76_mcu_timing_status mt76_dcoc_capture(struct mt76_dcoc *s,const struct mt76_mcu_timing_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange)return MT76_TIMING_REFUSED;
    s->attempted=true;enum mt76_mcu_timing_status status;
#define DCOC_READ(a,v) do{status=dcoc_read(s,io,a,v);if(status!=MT76_TIMING_OK)return status;}while(0)
#define DCOC_GUARD(m,c) do{DCOC_READ(0x1004,m);if(*(m))return MT76_TIMING_STATE;DCOC_READ(0x2000,c);if(*(c)!=0xf000f200)return MT76_TIMING_STATE;}while(0)
    for(unsigned stage=0;stage<2;stage++){
        DCOC_GUARD(&s->mac[stage*2],&s->core[stage*2]);
        for(unsigned i=0;i<9;i++)DCOC_READ(dcoc_plan[i].address,stage?&s->after[i]:&s->before[i]);
        DCOC_GUARD(&s->mac[stage*2+1],&s->core[stage*2+1]);
        if(!stage){
            if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
            uint8_t packet[80],reply[MT76_MCU_RESPONSE_MAX];
            if(mt76_dcoc_packet(packet,sizeof packet)!=80)return MT76_TIMING_REFUSED;
            s->command_attempted=true;int received=io->exchange(io->context,packet,80,reply,sizeof reply);
            if(received<0)return MT76_TIMING_IO;
            if(received!=16)return MT76_TIMING_REPLY;
            const uint32_t words[]={0x4a070008,0x412cbc,0x10f0,0x4a070008};
            for(unsigned i=0;i<16;i++)if(reply[i]!=(uint8_t)(words[i/4]>>(8*(i%4))))return MT76_TIMING_REPLY;
            s->ack_validated=true;
        }
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    for(unsigned i=0;i<9;i++)if(s->after[i]!=dcoc_plan[i].value)s->mismatches++;
    s->complete=true;return MT76_TIMING_OK;
#undef DCOC_READ
#undef DCOC_GUARD
}

/* Fixed shared-key reset only. No WCID/pairwise key/IV or RF admission. */
bool mt76_skey_request(unsigned op,bool *write,uint32_t *address,unsigned *length){
    if(op>=576 || !write || !address || !length)return false;
    unsigned slot=op/9,step=op%9,vif=slot/4,key=slot%4;
    *write=step==3 || step==5;*length=(step==5 || step==6)?32:4;
    if(step==0 || step==7)*address=0x1004;
    else if(step==1 || step==8)*address=0x2000;
    else if(step==5 || step==6)*address=(vif&8?0xb400:0xac00)+(4*(vif&7)+key)*32;
    else *address=(vif&8?0xb3f0:0xb000)+((vif&7)/2)*4;
    return true;
}
bool mt76_skey_transfer_valid(const struct mt76_skey *s,unsigned op,bool write,uint32_t address,const uint8_t *data,unsigned length){
    bool w;uint32_t a;unsigned n;
    if(!s || !s->attempted || s->complete || !data || s->operations!=op || s->slots!=op/9 ||
        !mt76_skey_request(op,&w,&a,&n) || write!=w || address!=a || length!=n)return false;
    if(write)for(unsigned i=0;i<n;i++)if(data[i]!=(op%9==3?(uint8_t)(s->mode_expected>>(8*i)):0))return false;
    return true;
}
enum mt76_mcu_timing_status mt76_skey_reset(struct mt76_skey *s,const struct mt76_skey_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->transfer)return MT76_TIMING_REFUSED;
    s->attempted=true;
    for(unsigned op=0;op<576;op++){
        bool write;uint32_t address;unsigned length;uint8_t data[32]={0};
        if(!mt76_skey_request(op,&write,&address,&length))return MT76_TIMING_REFUSED;
        if(op%9==3)for(unsigned i=0;i<4;i++)data[i]=(uint8_t)(s->mode_expected>>(8*i));
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        bool ok=io->transfer(io->context,op,write,address,data,length);
        uint32_t value=0;for(unsigned i=0;i<4;i++)value|=(uint32_t)data[i]<<(8*i);
        bool zero=true;for(unsigned i=0;i<length;i++)if(data[i])zero=false;
        for(unsigned i=0;i<32;i++)((volatile uint8_t *)data)[i]=0;
        if(!ok)return MT76_TIMING_IO;
        s->operations++;if(write)s->writes++;else s->reads++;
        switch(op%9){
        case 0:case 7:if(value)return MT76_TIMING_STATE;break;
        case 1:case 8:if(value!=0xf000f200)return MT76_TIMING_STATE;break;
        case 2:{
            if(value==UINT32_MAX)return MT76_TIMING_STATE;
            unsigned slot=op/9,shift=4*(slot%4+4*((slot/4)&1));s->mode_expected=value&~(15u<<shift);break;
        }
        case 4:if(value!=s->mode_expected)return MT76_TIMING_STATE;s->mode_verified++;break;
        case 6:if(!zero)return MT76_TIMING_STATE;s->key_zero++;break;
        default:break;
        }
        if(op%9==8)s->slots++;
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

/* Reference WCID reset: 256 attributes, only the first 128 address entries. */
static unsigned wcid_slot(unsigned op){return op<1024?op/8:128+(op-1024)/6;}
static unsigned wcid_step(unsigned op){return op<1024?op%8:(op-1024)%6;}
bool mt76_wcid_request(unsigned op,bool *write,uint32_t *address,unsigned *length){
    if(op>=1792 || !write || !address || !length)return false;
    unsigned slot=wcid_slot(op),step=wcid_step(op),last=slot<128?7:5;
    *write=step==2 || (slot<128 && step==4);*length=(slot<128 && (step==4 || step==5))?8:4;
    if(step==0 || step==last-1)*address=0x1004;
    else if(step==1 || step==last)*address=0x2000;
    else if(step==2 || step==3)*address=0xa800+slot*4;
    else *address=0x1800+slot*8;
    return true;
}
bool mt76_wcid_transfer_valid(const struct mt76_wcid *s,unsigned op,bool write,uint32_t address,const uint8_t *data,unsigned length){
    bool w;uint32_t a;unsigned n;
    if(!s || !s->attempted || s->complete || !data || s->operations!=op || s->slots!=wcid_slot(op) ||
        !mt76_wcid_request(op,&w,&a,&n) || write!=w || address!=a || length!=n)return false;
    if(write)for(unsigned i=0;i<n;i++)if(data[i])return false;
    return true;
}
enum mt76_mcu_timing_status mt76_wcid_reset(struct mt76_wcid *s,const struct mt76_wcid_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->transfer)return MT76_TIMING_REFUSED;
    s->attempted=true;
    for(unsigned op=0;op<1792;op++){
        bool write;uint32_t address;unsigned length;uint8_t data[8]={0};
        if(!mt76_wcid_request(op,&write,&address,&length))return MT76_TIMING_REFUSED;
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        bool ok=io->transfer(io->context,op,write,address,data,length);
        uint32_t value=0;for(unsigned i=0;i<4;i++)value|=(uint32_t)data[i]<<(8*i);
        bool zero=true;for(unsigned i=0;i<length;i++)if(data[i])zero=false;
        for(unsigned i=0;i<8;i++)((volatile uint8_t *)data)[i]=0;
        if(!ok)return MT76_TIMING_IO;
        s->operations++;if(write)s->writes++;else s->reads++;
        unsigned slot=wcid_slot(op),step=wcid_step(op),last=slot<128?7:5;
        if(step==0 || step==last-1){if(value)return MT76_TIMING_STATE;}
        else if(step==1 || step==last){if(value!=0xf000f200)return MT76_TIMING_STATE;}
        else if(!write){
            if(!zero)return MT76_TIMING_STATE;
            if(step==3)s->attributes_zero++;else s->addresses_zero++;
        }
        if(step==last)s->slots++;
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

/* Fixed post-WCID observations. Target values do not authorize writes. */
static const uint32_t mac_tail_addresses[22]={0x102c,0x208,0x1380,0x1608,0x1204,0x124,0xa44,0x1218,0x1220,0x13b0,0x13c0,0x1314,0x1318,0x131c,0x1320,0x1324,0x13d4,0x13d8,0x13dc,0x1238,0x6c,0x70};
uint32_t mt76_mac_tail_address(unsigned i){return i<22?mac_tail_addresses[i]:UINT32_MAX;}
uint32_t mt76_mac_tail_request(unsigned op){
    if(op>=26)return UINT32_MAX;
    if(op==0 || op==24)return 0x1004;
    if(op==1 || op==25)return 0x2000;
    return mac_tail_addresses[op-2];
}
enum mt76_mcu_timing_status mt76_mac_tail_capture(struct mt76_mac_tail *s,const struct mt76_mcu_timing_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->read)return MT76_TIMING_REFUSED;
    s->attempted=true;
    for(unsigned op=0;op<26;op++){
        uint32_t address=mt76_mac_tail_request(op),*value;
        if(op==0 || op==24)value=&s->mac[op==24];
        else if(op==1 || op==25)value=&s->core[op==25];
        else value=&s->values[op-2];
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        if(!io->read(io->context,address,value))return MT76_TIMING_IO;
        s->reads++;
        if((address==0x1004 && *value) || (address==0x2000 && *value!=0xf000f200))return MT76_TIMING_STATE;
        if(*value==UINT32_MAX)s->all_ones++;
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

/* Fixed PA mode, attenuation and coexistence initialization; no radio enable. */
static const uint32_t mac_fixed_addresses[4]={0x1218,0x1220,0x13c0,0x1238};
static const uint32_t mac_fixed_initial[4]={0x550055,0x550055,0x18181818,0x3700c8};
static const uint32_t mac_fixed_values[4]={0x500055,0x500055,0,0x1700c8};
uint32_t mt76_mac_fixed_address(unsigned i){return i<4?mac_fixed_addresses[i]:UINT32_MAX;}
uint32_t mt76_mac_fixed_initial(unsigned i){return i<4?mac_fixed_initial[i]:UINT32_MAX;}
uint32_t mt76_mac_fixed_value(unsigned i){return i<4?mac_fixed_values[i]:UINT32_MAX;}
bool mt76_mac_fixed_request(unsigned op,bool *write,uint32_t *address,unsigned *length){
    if(op>=32 || !write || !address || !length)return false;
    unsigned step=op%8;*write=step==4;*length=4;
    *address=step==0 || step==6?0x1004:step==1 || step==7?0x2000:step==2?0x208:mac_fixed_addresses[op/8];
    return true;
}
bool mt76_mac_fixed_transfer_valid(const struct mt76_mac_fixed *s,unsigned op,bool write,uint32_t address,const uint8_t *data,unsigned length){
    bool w;uint32_t a;unsigned n;
    if(!s || !s->attempted || s->complete || !data || s->operations!=op ||
        !mt76_mac_fixed_request(op,&w,&a,&n) || write!=w || address!=a || length!=n)return false;
    if(write){
        unsigned i=op/8;if(s->before[i]!=mac_fixed_initial[i] || s->dma[i]&15 || s->mac[i*2] || s->core[i*2]!=0xf000f200)return false;
        for(unsigned j=0;j<4;j++)if(data[j]!=(uint8_t)(mac_fixed_values[i]>>(8*j)))return false;
    }
    return true;
}
enum mt76_mcu_timing_status mt76_mac_fixed_apply(struct mt76_mac_fixed *s,const struct mt76_mac_fixed_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->transfer)return MT76_TIMING_REFUSED;
    s->attempted=true;
    for(unsigned op=0;op<32;op++){
        bool write;uint32_t address;unsigned length,i=op/8,step=op%8;uint8_t data[4]={0};
        if(!mt76_mac_fixed_request(op,&write,&address,&length))return MT76_TIMING_REFUSED;
        if(write)for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(mac_fixed_values[i]>>(8*j));
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        if(!io->transfer(io->context,op,write,address,data,length))return MT76_TIMING_IO;
        uint32_t value=0;for(unsigned j=0;j<4;j++)value|=(uint32_t)data[j]<<(8*j);
        s->operations++;if(write)s->writes++;else s->reads++;
        switch(step){
        case 0:case 6:s->mac[i*2+(step==6)]=value;if(value)return MT76_TIMING_STATE;break;
        case 1:case 7:s->core[i*2+(step==7)]=value;if(value!=0xf000f200)return MT76_TIMING_STATE;break;
        case 2:s->dma[i]=value;if(value&15)return MT76_TIMING_STATE;break;
        case 3:s->before[i]=value;if(value!=mac_fixed_initial[i])return MT76_TIMING_STATE;break;
        case 5:s->after[i]=value;if(value!=mac_fixed_values[i])return MT76_TIMING_STATE;s->verified++;break;
        default:break;
        }
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

/* Captured board power/IO prerequisites. Fixed allowlist, no RF/MAC/DMA enable. */
static const uint32_t mac_board_addresses[4]={0x1204,0x124,0x6c,0x70};
static const uint32_t mac_board_initial[4]={0xa,0xa0140280,0xa647a4,0x6b006868};
static const uint32_t mac_board_values[4]={0,0xa0040080,0xa647b6,0x6b006464};
uint32_t mt76_mac_board_address(unsigned i){return i<4?mac_board_addresses[i]:UINT32_MAX;}
uint32_t mt76_mac_board_initial(unsigned i){return i<4?mac_board_initial[i]:UINT32_MAX;}
uint32_t mt76_mac_board_value(unsigned i){return i<4?mac_board_values[i]:UINT32_MAX;}
bool mt76_mac_board_request(unsigned op,bool *write,uint32_t *address,unsigned *length){
    if(op>=32 || !write || !address || !length)return false;
    unsigned step=op%8;*write=step==4;*length=4;
    *address=step==0 || step==6?0x1004:step==1 || step==7?0x2000:step==2?0x208:mac_board_addresses[op/8];
    return true;
}
bool mt76_mac_board_transfer_valid(const struct mt76_mac_board *s,unsigned op,bool write,uint32_t address,const uint8_t *data,unsigned length){
    bool w;uint32_t a;unsigned n;
    if(!s || !s->attempted || s->complete || !data || s->operations!=op ||
        !mt76_mac_board_request(op,&w,&a,&n) || write!=w || address!=a || length!=n)return false;
    if(write){
        unsigned i=op/8;if(s->before[i]!=mac_board_initial[i] || s->dma[i]&15 || s->mac[i*2] || s->core[i*2]!=0xf000f200)return false;
        for(unsigned j=0;j<4;j++)if(data[j]!=(uint8_t)(mac_board_values[i]>>(8*j)))return false;
    }
    return true;
}
enum mt76_mcu_timing_status mt76_mac_board_apply(struct mt76_mac_board *s,const struct mt76_mac_board_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->transfer)return MT76_TIMING_REFUSED;
    s->attempted=true;
    for(unsigned op=0;op<32;op++){
        bool write;uint32_t address;unsigned length,i=op/8,step=op%8;uint8_t data[4]={0};
        if(!mt76_mac_board_request(op,&write,&address,&length))return MT76_TIMING_REFUSED;
        if(write)for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(mac_board_values[i]>>(8*j));
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        if(!io->transfer(io->context,op,write,address,data,length))return MT76_TIMING_IO;
        uint32_t value=0;for(unsigned j=0;j<4;j++)value|=(uint32_t)data[j]<<(8*j);
        s->operations++;if(write)s->writes++;else s->reads++;
        switch(step){
        case 0:case 6:s->mac[i*2+(step==6)]=value;if(value)return MT76_TIMING_STATE;break;
        case 1:case 7:s->core[i*2+(step==7)]=value;if(value!=0xf000f200)return MT76_TIMING_STATE;break;
        case 2:s->dma[i]=value;if(value&15)return MT76_TIMING_STATE;break;
        case 3:s->before[i]=value;if(value!=mac_board_initial[i])return MT76_TIMING_STATE;break;
        case 5:s->after[i]=value;if(value!=mac_board_values[i])return MT76_TIMING_STATE;s->verified++;break;
        default:break;
        }
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

/* Observe antenna prerequisites; this plan has no write callback. */
static const uint32_t ant_snapshot_addresses[5]={0x80,0x20,0x104,0x40,0x4c};
uint32_t mt76_ant_snapshot_address(unsigned i){return i<5?ant_snapshot_addresses[i]:UINT32_MAX;}
uint32_t mt76_ant_snapshot_request(unsigned op){
    if(op>=11)return UINT32_MAX;
    if(op==0 || op==9)return 0x1004;
    if(op==1 || op==10)return 0x2000;
    if(op==2 || op==8)return 0x208;
    return ant_snapshot_addresses[op-3];
}
enum mt76_mcu_timing_status mt76_ant_snapshot_capture(struct mt76_ant_snapshot *s,const struct mt76_mcu_timing_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->read)return MT76_TIMING_REFUSED;
    s->attempted=true;
    for(unsigned op=0;op<11;op++){
        uint32_t address=mt76_ant_snapshot_request(op),*value;
        if(op==0 || op==9)value=&s->mac[op==9];
        else if(op==1 || op==10)value=&s->core[op==10];
        else if(op==2 || op==8)value=&s->dma[op==8];
        else value=&s->values[op-3];
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        if(!io->read(io->context,address,value))return MT76_TIMING_IO;
        s->reads++;
        if(*value==UINT32_MAX || (address==0x1004 && *value) ||
           (address==0x2000 && *value!=0xf000f200) || (address==0x208 && (*value&15)))return MT76_TIMING_STATE;
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

/* Captured single-antenna setup. Fresh mapped fields must be authorized by the caller. */
static const uint32_t ant_init_addresses[5]={0x80,0x20,0x104,0x40,0x4c};
static const uint32_t ant_init_initial[5]={0xff000003,0xc01fff,0x33ffef,0x6f008847,1};
static const uint32_t ant_init_values[5]={0xff000003,0xc007ff,0x33ffef,0x6f008843,0x19};
uint32_t mt76_ant_init_address(unsigned i){return i<5?ant_init_addresses[i]:UINT32_MAX;}
uint32_t mt76_ant_init_initial(unsigned i){return i<5?ant_init_initial[i]:UINT32_MAX;}
uint32_t mt76_ant_init_value(unsigned i){return i<5?ant_init_values[i]:UINT32_MAX;}
bool mt76_ant_init_request(unsigned op,bool *write,uint32_t *address,unsigned *length){
    if(op>=40 || !write || !address || !length)return false;
    unsigned step=op%8;*write=step==4;*length=4;
    *address=step==0 || step==6?0x1004:step==1 || step==7?0x2000:step==2?0x208:ant_init_addresses[op/8];
    return true;
}
bool mt76_ant_init_transfer_valid(const struct mt76_ant_init *s,unsigned op,bool write,uint32_t address,const uint8_t *data,unsigned length){
    bool w;uint32_t a;unsigned n;
    if(!s || !s->attempted || s->complete || !data || s->operations!=op ||
        !mt76_ant_init_request(op,&w,&a,&n) || write!=w || address!=a || length!=n)return false;
    if(write){
        unsigned i=op/8;if(s->before[i]!=ant_init_initial[i] || s->dma[i]&15 || s->mac[i*2] || s->core[i*2]!=0xf000f200)return false;
        for(unsigned j=0;j<4;j++)if(data[j]!=(uint8_t)(ant_init_values[i]>>(8*j)))return false;
    }
    return true;
}
enum mt76_mcu_timing_status mt76_ant_init_apply(struct mt76_ant_init *s,const struct mt76_ant_init_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->transfer)return MT76_TIMING_REFUSED;
    s->attempted=true;
    for(unsigned op=0;op<40;op++){
        bool write;uint32_t address;unsigned length,i=op/8,step=op%8;uint8_t data[4]={0};
        if(!mt76_ant_init_request(op,&write,&address,&length))return MT76_TIMING_REFUSED;
        if(write)for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(ant_init_values[i]>>(8*j));
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        if(!io->transfer(io->context,op,write,address,data,length))return MT76_TIMING_IO;
        uint32_t value=0;for(unsigned j=0;j<4;j++)value|=(uint32_t)data[j]<<(8*j);
        s->operations++;if(write)s->writes++;else s->reads++;
        switch(step){
        case 0:case 6:s->mac[i*2+(step==6)]=value;if(value)return MT76_TIMING_STATE;break;
        case 1:case 7:s->core[i*2+(step==7)]=value;if(value!=0xf000f200)return MT76_TIMING_STATE;break;
        case 2:s->dma[i]=value;if(value&15)return MT76_TIMING_STATE;break;
        case 3:s->before[i]=value;if(value!=ant_init_initial[i])return MT76_TIMING_STATE;break;
        case 5:s->after[i]=value;if(value!=ant_init_values[i])return MT76_TIMING_STATE;s->verified++;break;
        default:break;
        }
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

/* Linux USB RF address = 0x80000000 + (bank<<16 | register).
 * Single read only; sequence8 follows the completed sequence7 DCOC exchange. */
size_t mt76_rf_probe_packet(uint8_t *out,size_t capacity) {
    static const uint8_t payload[8]={0x16,0,0,0x80,0,0,0,0};
    return mt76_mcu_encode(10,8,payload,sizeof payload,out,capacity);
}
uint32_t mt76_rf_probe_read_address(unsigned i) {
    static const uint32_t addresses[3]={0x1004,0x2000,0x208};
    return i<6?addresses[i%3]:UINT32_MAX;
}
enum mt76_mcu_timing_status mt76_rf_probe_apply(struct mt76_rf_probe *s,
    const struct mt76_mcu_timing_io *io) {
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange)return MT76_TIMING_REFUSED;
    *s=(struct mt76_rf_probe){.attempted=true,.received=-1};
    for(unsigned boundary=0;boundary<2;boundary++) {
        uint32_t *values[3]={&s->mac[boundary],&s->core[boundary],&s->dma[boundary]};
        for(unsigned i=0;i<3;i++) {
            if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
            uint32_t address=mt76_rf_probe_read_address(s->reads++);
            if(!io->read(io->context,address,values[i]))return MT76_TIMING_IO;
            uint32_t v=*values[i];
            if((i==0 && v) || (i==1 && v!=0xf000f200) || (i==2 && (v==UINT32_MAX || (v&15))))return MT76_TIMING_STATE;
        }
        if(boundary)break;
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX];
        if(mt76_rf_probe_packet(packet,sizeof packet)!=16)return MT76_TIMING_STATE;
        s->request_attempted=true;
        s->received=io->exchange(io->context,packet,sizeof packet,reply,sizeof reply);
        if(s->received<0)return MT76_TIMING_IO;
        for(unsigned i=0;i<4 && (int)(4*i+4)<=s->received;i++)
            for(unsigned b=0;b<4;b++)s->words[i]|=(uint32_t)reply[4*i+b]<<(8*b);
        if(s->received!=16 || s->words[0]!=0x4a080008 || s->words[1]!=0x80000016 ||
           s->words[2]>255 || s->words[3]!=0x4a080008)return MT76_TIMING_REPLY;
        s->value=(uint8_t)s->words[2];s->reply_valid=true;
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

/* Numeric MT7610 USB central RF settings, Linux v7.0 initvals_phy.h/phy.c.
 * Register0:22 trim and the subsequent DAC/VCO calibration sequence are absent.
 * No Linux implementation is imported; this fixed admission engine is local. */
static const uint8_t rf_central_regs[MT76_RF_CENTRAL_COUNT]={
    1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,19,20,21,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,64,65,66,67,68,69,70,71,72,73
};
static const uint8_t rf_central_values[MT76_RF_CENTRAL_COUNT]={
    0x01,0x11,0x73,0x30,0x00,0x41,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x00,0x00,0x20,0x22,0x12,0x00,0x33,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2f,0x00,0x80,0x01,0x04,0x00,0x08,0x08,0x40,0xd0,0x93
};
uint32_t mt76_rf_central_address(unsigned i){return i<MT76_RF_CENTRAL_COUNT?0x80000000u|rf_central_regs[i]:UINT32_MAX;}
unsigned mt76_rf_central_value(unsigned i){return i<MT76_RF_CENTRAL_COUNT?rf_central_values[i]:UINT32_MAX;}
unsigned mt76_rf_central_sequence(unsigned i,bool write){return i<MT76_RF_CENTRAL_COUNT?(8+2*i+(!write))%15+1:0;}
uint32_t mt76_rf_central_guard_address(unsigned op){
    static const uint32_t addresses[3]={0x1004,0x2000,0x208};
    unsigned step=op%8;
    return op<MT76_RF_CENTRAL_OPS && (step<3 || step>=5)?addresses[step<3?step:step-5]:UINT32_MAX;
}
size_t mt76_rf_central_packet(unsigned op,uint8_t *out,size_t capacity){
    unsigned i=op/8,step=op%8;uint8_t payload[8]={0};
    if(i>=MT76_RF_CENTRAL_COUNT || (step!=3 && step!=4))return 0;
    uint32_t address=mt76_rf_central_address(i);
    for(unsigned b=0;b<4;b++)payload[b]=(uint8_t)(address>>(8*b));
    if(step==3)payload[4]=rf_central_values[i];
    return mt76_mcu_encode(step==3?12:10,mt76_rf_central_sequence(i,step==3),payload,8,out,capacity);
}
static bool rf_central_guard_valid(unsigned n,uint32_t v){
    return n==0?v==0:n==1?v==0xf000f200:v!=UINT32_MAX && !(v&15);
}
bool mt76_rf_central_admit(const struct mt76_rf_central *s,unsigned op){
    if(!s || !s->attempted || s->complete || op>=MT76_RF_CENTRAL_OPS || s->operations!=op)return false;
    unsigned i=op/8,step=op%8;
    if(s->reads!=i*6+(step<3?step:3)+(step>5?step-5:0) ||
       s->exchanges!=i*2+(step>3)+(step>4) || s->acks!=i+(step>3) || s->verified!=i+(step>4))return false;
    /* Recheck completed proof, not merely the aggregate counts. */
    for(unsigned j=0;j<i;j++){
        if(s->values[j]!=rf_central_values[j])return false;
        for(unsigned b=0;b<2;b++)for(unsigned n=0;n<3;n++)
            if(!rf_central_guard_valid(n,s->guards[2*j+b][n]))return false;
    }
    for(unsigned n=0;n<3;n++){
        if(step>n && !rf_central_guard_valid(n,s->guards[2*i][n]))return false;
        if(step>5+n && !rf_central_guard_valid(n,s->guards[2*i+1][n]))return false;
    }
    if(step>4 && s->values[i]!=rf_central_values[i])return false;
    if(step>=4){
        uint32_t h=0x4a000008u|(mt76_rf_central_sequence(i,step==4)<<16);
        if(s->received!=16 || s->words[0]!=h || s->words[3]!=h ||
           s->words[1]!=mt76_rf_central_address(i) || s->words[2]!=rf_central_values[i])return false;
    }
    return true;
}
enum mt76_mcu_timing_status mt76_rf_central_apply(struct mt76_rf_central *s,const struct mt76_mcu_timing_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange)return MT76_TIMING_REFUSED;
    *s=(struct mt76_rf_central){.attempted=true,.received=-1};
    for(unsigned op=0;op<MT76_RF_CENTRAL_OPS;op++){
        if(!mt76_rf_central_admit(s,op))return MT76_TIMING_STATE;
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        unsigned i=op/8,step=op%8;
        if(step<3 || step>=5){
            unsigned boundary=2*i+(step>=5),n=step<3?step:step-5;
            if(!io->read(io->context,mt76_rf_central_guard_address(op),&s->guards[boundary][n]))return MT76_TIMING_IO;
            s->reads++;s->operations++;
            if(!rf_central_guard_valid(n,s->guards[boundary][n]))return MT76_TIMING_STATE;
        }else{
            uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX];
            if(mt76_rf_central_packet(op,packet,sizeof packet)!=16)return MT76_TIMING_STATE;
            s->received=io->exchange(io->context,packet,sizeof packet,reply,sizeof reply);s->exchanges++;
            for(unsigned n=0;n<4;n++){
                s->words[n]=0;
                if(s->received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)s->words[n]|=(uint32_t)reply[4*n+b]<<(8*b);
            }
            if(s->received<0)return MT76_TIMING_IO;
            uint32_t h=0x4a000008u|(mt76_rf_central_sequence(i,step==3)<<16);
            if(s->received!=16 || s->words[0]!=h || s->words[3]!=h || s->words[1]!=mt76_rf_central_address(i) || s->words[2]>255)return MT76_TIMING_REPLY;
            if(s->words[2]!=rf_central_values[i])return MT76_TIMING_STATE;
            if(step==3)s->acks++;else{s->values[i]=(uint8_t)s->words[2];s->verified++;}
            s->operations++;
        }
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

bool mt76_rf_six_prefix_valid(const struct mt76_rf_central *s,enum mt76_mcu_timing_status status){
    if(!s || !s->attempted || s->complete || s->reads!=33 || s->exchanges!=12 || s->acks!=6 || s->received!=16 ||
       s->words[0]!=0x4a050008 || s->words[1]!=0x80000006 || s->words[2]>255 || s->words[3]!=0x4a050008)return false;
    if(s->words[2]==0x41){
        if(status!=MT76_TIMING_CANCELLED || s->operations!=45 || s->verified!=6 || s->values[5]!=0x41)return false;
    }else if(status!=MT76_TIMING_STATE || s->operations!=44 || s->verified!=5)return false;
    for(unsigned i=0;i<5;i++)if(s->values[i]!=rf_central_values[i])return false;
    for(unsigned i=0;i<11;i++)for(unsigned n=0;n<3;n++)if(!rf_central_guard_valid(n,s->guards[i][n]))return false;
    return true;
}
static const uint8_t rf_six_regs[5]={6,1,2,6,22};
uint32_t mt76_rf_six_guard_address(unsigned op){
    static const uint32_t addresses[3]={0x1004,0x2000,0x208};
    if(op>=23 || (op>=3 && (op-3)%4==0))return UINT32_MAX;
    return addresses[op<3?op:(op-4)%4];
}
size_t mt76_rf_six_packet(unsigned op,uint8_t *out,size_t capacity){
    if(op<3 || op>=23 || (op-3)%4)return 0;
    unsigned i=(op-3)/4;uint8_t payload[8]={rf_six_regs[i],0,0,0x80,0,0,0,0};
    return mt76_mcu_encode(10,6+i,payload,8,out,capacity);
}
static bool rf_six_reply_valid(const struct mt76_rf_six *s,unsigned i){
    uint32_t h=0x4a000008u|((6+i)<<16),v=s->words[i][2];
    return s->words[i][0]==h && s->words[i][1]==(0x80000000u|rf_six_regs[i]) && s->words[i][3]==h &&
        v<=255 && s->values[i]==v && (i==0 || i==3 || v==(i==1?1u:i==2?17u:0u));
}
bool mt76_rf_six_admit(const struct mt76_rf_six *s,unsigned op){
    if(!s || !s->attempted || s->complete || op>=23 || s->operations!=op)return false;
    unsigned requests=op<4?0:(op-4)/4+1;
    if(s->requests!=requests || s->reads!=op-requests)return false;
    for(unsigned i=0;i<requests;i++)if(!rf_six_reply_valid(s,i))return false;
    for(unsigned i=0;i<op;i++)if(mt76_rf_six_guard_address(i)!=UINT32_MAX){
        unsigned b=i<3?0:(i-4)/4+1,n=i<3?i:(i-4)%4;
        if(!rf_central_guard_valid(n,s->guards[b][n]))return false;
    }
    return requests==0 || s->received==16;
}
enum mt76_mcu_timing_status mt76_rf_six_apply(struct mt76_rf_six *s,const struct mt76_mcu_timing_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange)return MT76_TIMING_REFUSED;
    *s=(struct mt76_rf_six){.attempted=true,.received=-1};
    for(unsigned op=0;op<23;op++){
        if(!mt76_rf_six_admit(s,op))return MT76_TIMING_STATE;
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        uint32_t address=mt76_rf_six_guard_address(op);
        if(address!=UINT32_MAX){
            unsigned b=op<3?0:(op-4)/4+1,n=op<3?op:(op-4)%4;
            if(!io->read(io->context,address,&s->guards[b][n]))return MT76_TIMING_IO;
            s->reads++;s->operations++;
            if(!rf_central_guard_valid(n,s->guards[b][n]))return MT76_TIMING_STATE;
        }else{
            unsigned i=(op-3)/4;uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX];
            if(mt76_rf_six_packet(op,packet,sizeof packet)!=16)return MT76_TIMING_STATE;
            s->received=io->exchange(io->context,packet,sizeof packet,reply,sizeof reply);s->requests++;
            for(unsigned n=0;n<4;n++)if(s->received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)s->words[i][n]|=(uint32_t)reply[4*n+b]<<(8*b);
            if(s->received<0)return MT76_TIMING_IO;
            uint32_t h=0x4a000008u|((6+i)<<16);
            if(s->received!=16 || s->words[i][0]!=h || s->words[i][1]!=(0x80000000u|rf_six_regs[i]) || s->words[i][3]!=h || s->words[i][2]>255)return MT76_TIMING_REPLY;
            s->values[i]=(uint8_t)s->words[i][2];
            if(!rf_six_reply_valid(s,i))return MT76_TIMING_STATE;
            s->operations++;
        }
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

bool mt76_rf_six_complete_valid(const struct mt76_rf_six *s){
    if(!s || !s->attempted || !s->complete || s->operations!=23 || s->reads!=18 || s->requests!=5 || s->received!=16)return false;
    for(unsigned i=0;i<5;i++)if(!rf_six_reply_valid(s,i))return false;
    for(unsigned b=0;b<6;b++)for(unsigned n=0;n<3;n++)if(!rf_central_guard_valid(n,s->guards[b][n]))return false;
    return true;
}
uint32_t mt76_rf_csr_selector(unsigned i){return i<5?0x80000000u|((uint32_t)rf_six_regs[i]<<8):UINT32_MAX;}
static bool rf_csr_result_valid(unsigned i,uint32_t v){
    if(i>=5 || (v&0xc003ff00u)!=(mt76_rf_csr_selector(i)&0x3ff00u))return false;
    unsigned value=v&255;
    return i==0 || i==3 || value==(i==1?1u:i==2?17u:0u);
}
bool mt76_rf_csr_request(const struct mt76_rf_csr *s,bool *write,uint32_t *address,uint32_t *value){
    if(!s || !write || !address || !value || !s->attempted || s->complete || s->index>=5 || s->phase>8)return false;
    unsigned ops=0,writes=0;
    for(unsigned i=0;i<=s->index;i++){
        unsigned phase=i<s->index?9:s->phase;
        unsigned p=s->polls[i][0],q=s->polls[i][1];
        if(p>32 || q>32 || (phase<3 && p) || (phase<5 && q))return false;
        if(phase==3 && p && (s->idle[i]==UINT32_MAX || !(s->idle[i]&0x80000000u)))return false;
        if(phase==5 && q && (s->result[i]==UINT32_MAX || !(s->result[i]&0x80000000u)))return false;
        if((phase==3 && p==32) || (phase==5 && q==32))return false;
        if(phase>3 && (!p || s->idle[i]&0x80000000u))return false;
        if(phase>5 && (!q || !rf_csr_result_valid(i,s->result[i])))return false;
        for(unsigned n=0;n<3;n++){
            if(phase>n && !rf_central_guard_valid(n,s->guards[2*i][n]))return false;
            if(phase>6+n && !rf_central_guard_valid(n,s->guards[2*i+1][n]))return false;
        }
        unsigned guard_reads=phase<3?phase:3;
        if(phase>6)guard_reads+=phase-6;
        ops+=guard_reads+p+q+(phase>4);writes+=phase>4;
    }
    if(s->operations!=ops || s->writes!=writes || s->reads!=ops-writes)return false;
    *write=s->phase==4;*value=*write?mt76_rf_csr_selector(s->index):0;
    *address=s->phase<3?mt76_rf_probe_read_address(s->phase):s->phase>=6?mt76_rf_probe_read_address(s->phase-6):0x500;
    return true;
}
enum mt76_mcu_timing_status mt76_rf_csr_apply(struct mt76_rf_csr *s,const struct mt76_rf_csr_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->transfer)return MT76_TIMING_REFUSED;
    *s=(struct mt76_rf_csr){.attempted=true};
    while(s->index<5){
        bool write;uint32_t address,value;
        if(!mt76_rf_csr_request(s,&write,&address,&value))return MT76_TIMING_STATE;
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        if(!io->transfer(io->context,s->operations,write,address,&value))return MT76_TIMING_IO;
        s->operations++;if(write)s->writes++;else s->reads++;
        unsigned i=s->index,phase=s->phase;
        if(phase<3 || phase>=6){
            unsigned n=phase<3?phase:phase-6;s->guards[2*i+(phase>=6)][n]=value;
            if(!rf_central_guard_valid(n,value))return MT76_TIMING_STATE;
            s->phase++;
        }else if(write)s->phase++;
        else{
            unsigned which=phase==5;
            s->polls[i][which]++;if(which)s->result[i]=value;else s->idle[i]=value;
            if(value==UINT32_MAX)return MT76_TIMING_STATE;
            if(value&0x80000000u){if(s->polls[i][which]==32)return MT76_TIMING_STATE;}
            else{if(which && !rf_csr_result_valid(i,value))return MT76_TIMING_STATE;s->phase++;}
        }
        if(s->phase==9){s->index++;s->phase=0;}
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

bool mt76_rf_csr_zero_valid(const struct mt76_rf_csr *s){
    if(!s || !s->attempted || !s->complete || s->index!=5 || s->phase || s->writes!=5 ||
       s->operations<45 || s->operations>355 || s->reads!=s->operations-5 ||
       (s->result[0]&255) || (s->result[3]&255) || !rf_central_guard_valid(2,s->guards[9][2]))return false;
    struct mt76_rf_csr before=*s;before.complete=false;before.index=4;before.phase=8;before.operations--;before.reads--;
    bool write;uint32_t address,value;return mt76_rf_csr_request(&before,&write,&address,&value);
}
bool mt76_rf_write_request(const struct mt76_rf_write *s,bool *write,uint32_t *address,uint32_t *value){
    if(!s || !write || !address || !value || !s->attempted || s->complete || s->phase>=14)return false;
    const unsigned phases[3]={3,5,10};unsigned guards=0,polls=0;
    for(unsigned i=0;i<3;i++){
        unsigned first=i==0?0:i==1?6:11;
        for(unsigned n=0;n<3;n++)if(s->phase>first+n){
            guards++;if(!rf_central_guard_valid(n,s->guards[i][n]))return false;
        }
        unsigned p=s->polls[i];uint32_t v=s->words[i];polls+=p;
        if(p>32 || (s->phase<phases[i] && p))return false;
        if(s->phase==phases[i] && p && (p==32 || v==UINT32_MAX || !(v&0x80000000u)))return false;
        if(s->phase>phases[i]){
            if(!p || (v&0x80000000u))return false;
            if(i && (v&(i==1?0x8003ff00u:0xc003ff00u))!=0x600)return false;
        }
    }
    unsigned writes=(s->phase>4)+(s->phase>9),ops=guards+polls+writes;
    if(s->operations!=ops || s->reads!=ops-writes || s->writes!=writes)return false;
    *write=s->phase==4 || s->phase==9;*value=s->phase==4?0xc0000641u:s->phase==9?0x80000600u:0;
    *address=s->phase<3?mt76_rf_probe_read_address(s->phase):
        s->phase>=6 && s->phase<9?mt76_rf_probe_read_address(s->phase-6):
        s->phase>=11?mt76_rf_probe_read_address(s->phase-11):0x500;
    return true;
}
enum mt76_mcu_timing_status mt76_rf_write_apply(struct mt76_rf_write *s,const struct mt76_rf_csr_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->transfer)return MT76_TIMING_REFUSED;
    *s=(struct mt76_rf_write){.attempted=true};
    while(s->phase<14){
        bool write;uint32_t address,value;
        if(!mt76_rf_write_request(s,&write,&address,&value))return MT76_TIMING_STATE;
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        if(!io->transfer(io->context,s->operations,write,address,&value))return MT76_TIMING_IO;
        s->operations++;if(write)s->writes++;else s->reads++;
        unsigned phase=s->phase;
        if(phase<3 || (phase>=6 && phase<9) || phase>=11){
            unsigned b=phase<3?0:phase<9?1:2,n=phase<3?phase:phase<9?phase-6:phase-11;
            s->guards[b][n]=value;if(!rf_central_guard_valid(n,value))return MT76_TIMING_STATE;s->phase++;
        }else if(write)s->phase++;
        else{
            unsigned i=phase==3?0:phase==5?1:2;s->polls[i]++;s->words[i]=value;
            if(value==UINT32_MAX)return MT76_TIMING_STATE;
            if(value&0x80000000u){if(s->polls[i]==32)return MT76_TIMING_STATE;}
            else{
                if(i && (value&(i==1?0x8003ff00u:0xc003ff00u))!=0x600)return MT76_TIMING_STATE;
                s->phase++;
            }
        }
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

static const uint32_t rf_state_addresses[MT76_RF_STATE_COUNT]={0x80,0x20,0x104,0x400,0x504,0x508,0x50c,0x518};
uint32_t mt76_rf_state_address(unsigned i){return i<MT76_RF_STATE_COUNT?rf_state_addresses[i]:UINT32_MAX;}
uint32_t mt76_rf_state_request(unsigned op){
    if(op>=14)return UINT32_MAX;
    return op<3?mt76_rf_probe_read_address(op):op>=11?mt76_rf_probe_read_address(op-11):rf_state_addresses[op-3];
}
bool mt76_rf_state_admit(const struct mt76_rf_state *s){
    if(!s || !s->attempted || s->complete || s->reads>=14)return false;
    for(unsigned op=0;op<s->reads;op++){
        if(op<3 || op>=11){unsigned b=op>=11,n=b?op-11:op;
            if(!rf_central_guard_valid(n,s->guards[b][n]))return false;
        }else if(s->values[op-3]==UINT32_MAX)return false;
    }
    return true;
}
enum mt76_mcu_timing_status mt76_rf_state_capture(struct mt76_rf_state *s,const struct mt76_mcu_timing_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->read)return MT76_TIMING_REFUSED;
    *s=(struct mt76_rf_state){.attempted=true};
    for(unsigned op=0;op<14;op++){
        if(!mt76_rf_state_admit(s))return MT76_TIMING_STATE;
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        uint32_t *v=op<3?&s->guards[0][op]:op>=11?&s->guards[1][op-11]:&s->values[op-3];
        if(!io->read(io->context,mt76_rf_state_request(op),v))return MT76_TIMING_IO;
        s->reads++;
        if(*v==UINT32_MAX || ((op<3 || op>=11) && !rf_central_guard_valid(op<3?op:op-11,*v)))return MT76_TIMING_STATE;
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

/* Fixed numeric bank5 table from pinned Linux v7.0; USB patch5:2 remains0c.
 * Plan selection is private; callers cannot supply counts, targets or values. */
static const uint8_t rf_bank5_regs[MT76_RF_BANK5_COUNT]={2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,127};
static const uint8_t rf_bank5_values[MT76_RF_BANK5_COUNT]={0xc,0x0,0x0,0x84,0x2,0x0,0x0,0x0,0x51,0x22,0x22,0xf,0x47,0x25,0xc7,0x0,0x0,0x30,0x33,0x2,0x32,0x0,0x25,0x0,0x12,0xf,0x0,0x51,0x35,0x31,0x31,0x34,0x3,0x0,0xdd,0xb3,0x33,0xb1,0x71,0xf2,0x47,0x77,0xe,0x10,0x0,0x53,0x3,0xef,0xc7,0x62,0x62,0x0,0x0,0xf,0xf,0x16,0x16,0x10,0x10,0xd0,0x6c,0x58,0x58,0xf2,0xe8,0xf0,0xf0,0x4};
/* Next fixed reference5G channel0 initialization table; no channel selection. */
static const uint8_t rf_bank6_regs[MT76_RF_BANK6_COUNT]={2,3,4,5,6,7,8,9,10,11,13,14,15,16,18,19,20,21,22,25,26,27,28,29,37,38,40,41,43,44,46,47,48,49,62,63,64,65};
static const uint8_t rf_bank6_values[MT76_RF_BANK6_COUNT]={0xc,0x0,0x0,0x84,0x2,0x0,0x0,0x0,0x0,0x1,0x23,0x0,0x4,0x22,0x8,0x0,0x0,0x0,0xfb,0x76,0x24,0x4,0x0,0x0,0xbb,0xb3,0x33,0x33,0x3,0xb3,0x17,0xe,0x10,0x7,0x0,0x0,0xf1,0xf};
/* Fixed reference VGA initialization in RF bank7; no band/channel switching. */
static const uint8_t rf_vga_regs[MT76_RF_VGA_COUNT]={0,1,2,3,4,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,61,72,73,74};
static const uint8_t rf_vga_values[MT76_RF_VGA_COUNT]={0x47,0x0,0x0,0x0,0x0,0x13,0xf,0x13,0x13,0x13,0x20,0x22,0x7c,0x0,0x0,0x0,0xf1,0x11,0xc2,0x41,0x20,0x40,0xd7,0xa2,0x60,0x49,0x20,0x44,0xc1,0x60,0xc0,0x1,0x3c,0x34,0x0};
/* Fixed default20MHz/G-band selection from reference RF init, in source order. */
static const uint32_t rf_g20_addresses[MT76_RF_G20_COUNT]={0x80000011u,0x80070006u,0x80070007u,0x80070008u,0x8007003au,0x8007003bu,0x8007003cu,0x8007004cu,0x8007004du,0x80000010u,0x80000012u,0x80000027u,0x8006007fu,0x80070005u,0x80070009u,0x80070046u,0x80070047u,0x8007004eu,0x8007004fu};
static const uint8_t rf_g20_values[MT76_RF_G20_COUNT]={0x0,0x40,0x40,0x3,0x40,0x40,0xaa,0x40,0x40,0x20,0x0,0x36,0x84,0x40,0x0,0x0,0x0,0x0,0x0};
static const uint32_t rf_trim_addresses[1]={0x80000016u};
static const uint8_t rf_trim_values[1]={0x44};
struct rf_table_plan {unsigned count,sequence_base;uint32_t base;const uint8_t *regs,*values;const uint32_t *addresses;};
static const struct rf_table_plan rf_central_plan={MT76_RF_CENTRAL_COUNT,8,0x80000000u,rf_central_regs,rf_central_values,NULL};
static const struct rf_table_plan rf_bank5_plan={MT76_RF_BANK5_COUNT,96,0x80050000u,rf_bank5_regs,rf_bank5_values,NULL};
static const struct rf_table_plan rf_bank6_plan={MT76_RF_BANK6_COUNT,232,0x80060000u,rf_bank6_regs,rf_bank6_values,NULL};
static const struct rf_table_plan rf_vga_plan={MT76_RF_VGA_COUNT,308,0x80070000u,rf_vga_regs,rf_vga_values,NULL};
static const struct rf_table_plan rf_g20_plan={MT76_RF_G20_COUNT,378,0,NULL,rf_g20_values,rf_g20_addresses};
static const struct rf_table_plan rf_trim_plan={1,416,0,NULL,rf_trim_values,rf_trim_addresses};
static uint32_t rf_plan_address(const struct rf_table_plan *p,unsigned i){return i<p->count?(p->addresses?p->addresses[i]:p->base|p->regs[i]):UINT32_MAX;}
static unsigned rf_plan_sequence(const struct rf_table_plan *p,unsigned command){return command<(p->count*2)?(p->sequence_base+command)%15+1:0;}
static uint32_t rf_plan_guard_address(const struct rf_table_plan *p,unsigned op){
    unsigned step=op%7;
    return op<(p->count*14) && step!=3?mt76_rf_probe_read_address(step<3?step:step-4):UINT32_MAX;
}
static size_t rf_plan_packet(const struct rf_table_plan *p,unsigned op,uint8_t *out,size_t capacity){
    if(op>=(p->count*14) || op%7!=3)return 0;
    unsigned command=op/7,i=command%p->count;uint8_t payload[8]={0};
    uint32_t address=rf_plan_address(p,i);
    for(unsigned b=0;b<4;b++)payload[b]=(uint8_t)(address>>(8*b));
    if(command<p->count)payload[4]=(uint8_t)p->values[i];
    return mt76_mcu_encode(command<p->count?12:10,rf_plan_sequence(p,command),payload,8,out,capacity);
}
static bool rf_plan_state_valid(const struct rf_table_plan *p,const struct mt76_rf_table *s,unsigned op){
    if(!s || !s->attempted || op>(p->count*14) || s->operations!=op)return false;
    unsigned command=op/7,step=op%7,exchanges=command+(step>3);
    unsigned acks=exchanges<p->count?exchanges:p->count;
    unsigned observed=exchanges-acks,equal=0;uint64_t mask=0,hi=0;
    if(s->reads!=command*6+(step<3?step:3)+(step>4?step-4:0) || s->exchanges!=exchanges ||
       s->acks!=acks || s->observed!=observed || s->received!=(exchanges?16:-1))return false;
    for(unsigned i=0;i<observed;i++){
        if(s->values[i]==p->values[i])equal++;else if(i<64)mask|=1ull<<i;else hi|=1ull<<(i-64);
    }
    if(s->equal!=equal || s->mismatch_mask!=mask || s->mismatch_hi!=hi)return false;
    for(unsigned q=0;q<=command;q++){
        unsigned done=q<command?7:step;
        for(unsigned n=0;n<3;n++){
            if(done>n && !rf_central_guard_valid(n,s->guards[2*q][n]))return false;
            if(done>4+n && !rf_central_guard_valid(n,s->guards[2*q+1][n]))return false;
        }
    }
    for(unsigned q=0;q<exchanges;q++){
        unsigned i=q%p->count;uint32_t h=0x4a000008u|(rf_plan_sequence(p,q)<<16);
        uint32_t value=q<p->count?p->values[i]:s->values[i];
        if(s->words[q][0]!=h || s->words[q][1]!=rf_plan_address(p,i) || s->words[q][2]!=value || s->words[q][3]!=h)return false;
    }
    return true;
}
static bool rf_plan_admit(const struct rf_table_plan *p,const struct mt76_rf_table *s,unsigned op){
    return s && !s->complete && op<p->count*14 && rf_plan_state_valid(p,s,op);
}
static enum mt76_mcu_timing_status rf_plan_capture(const struct rf_table_plan *p,struct mt76_rf_table *s,const struct mt76_mcu_timing_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange)return MT76_TIMING_REFUSED;
    *s=(struct mt76_rf_table){.attempted=true,.received=-1};
    for(unsigned op=0;op<(p->count*14);op++){
        if(!rf_plan_admit(p,s,op))return MT76_TIMING_STATE;
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        unsigned command=op/7,step=op%7,i=command%p->count;
        if(step!=3){
            unsigned edge=2*command+(step>=4),n=step<3?step:step-4;uint32_t *v=&s->guards[edge][n];
            if(!io->read(io->context,rf_plan_guard_address(p,op),v))return MT76_TIMING_IO;
            s->reads++;s->operations++;
            if(!rf_central_guard_valid(n,*v))return MT76_TIMING_STATE;
        }else{
            uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX];
            if(rf_plan_packet(p,op,packet,sizeof packet)!=16)return MT76_TIMING_STATE;
            s->received=io->exchange(io->context,packet,sizeof packet,reply,sizeof reply);s->exchanges++;
            for(unsigned n=0;n<4;n++)if(s->received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)s->words[command][n]|=(uint32_t)reply[4*n+b]<<(8*b);
            if(s->received<0)return MT76_TIMING_IO;
            uint32_t h=0x4a000008u|(rf_plan_sequence(p,command)<<16),value=s->words[command][2];
            if(s->received!=16 || s->words[command][0]!=h || s->words[command][1]!=rf_plan_address(p,i) || value>255 || s->words[command][3]!=h)return MT76_TIMING_REPLY;
            if(command<p->count){
                if(value!=p->values[i])return MT76_TIMING_REPLY;
                s->acks++;
            }else{
                s->values[i]=(uint8_t)value;s->observed++;
                if(value==p->values[i])s->equal++;else if(i<64)s->mismatch_mask|=1ull<<i;else s->mismatch_hi|=1ull<<(i-64);
            }
            s->operations++;
        }
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

unsigned mt76_rf_table_sequence(unsigned command){return rf_plan_sequence(&rf_central_plan,command);}
uint32_t mt76_rf_table_guard_address(unsigned op){return rf_plan_guard_address(&rf_central_plan,op);}
size_t mt76_rf_table_packet(unsigned op,uint8_t *out,size_t capacity){return rf_plan_packet(&rf_central_plan,op,out,capacity);}
bool mt76_rf_table_admit(const struct mt76_rf_table *state,unsigned op){return rf_plan_admit(&rf_central_plan,state,op);}
enum mt76_mcu_timing_status mt76_rf_table_capture(struct mt76_rf_table *state,const struct mt76_mcu_timing_io *io){return rf_plan_capture(&rf_central_plan,state,io);}

unsigned mt76_rf_bank5_sequence(unsigned command){return rf_plan_sequence(&rf_bank5_plan,command);}
uint32_t mt76_rf_bank5_guard_address(unsigned op){return rf_plan_guard_address(&rf_bank5_plan,op);}
size_t mt76_rf_bank5_packet(unsigned op,uint8_t *out,size_t capacity){return rf_plan_packet(&rf_bank5_plan,op,out,capacity);}
bool mt76_rf_bank5_admit(const struct mt76_rf_table *state,unsigned op){return rf_plan_admit(&rf_bank5_plan,state,op);}
enum mt76_mcu_timing_status mt76_rf_bank5_capture(struct mt76_rf_table *state,const struct mt76_mcu_timing_io *io){return rf_plan_capture(&rf_bank5_plan,state,io);}
uint32_t mt76_rf_bank5_address(unsigned i){return rf_plan_address(&rf_bank5_plan,i);}
unsigned mt76_rf_bank5_value(unsigned i){return i<MT76_RF_BANK5_COUNT?rf_bank5_values[i]:UINT32_MAX;}
bool mt76_rf_table_zero_profile_valid(const struct mt76_rf_table *s){
    if(!s || !s->complete || s->operations!=MT76_RF_TABLE_OPS || !rf_plan_state_valid(&rf_central_plan,s,MT76_RF_TABLE_OPS) ||
       s->equal!=43 || s->mismatch_mask!=(1ull<<5))return false;
    for(unsigned i=0;i<MT76_RF_CENTRAL_COUNT;i++)if(s->values[i]!=(i==5?0:mt76_rf_central_value(i)))return false;
    return true;
}

unsigned mt76_rf_bank6_sequence(unsigned command){return rf_plan_sequence(&rf_bank6_plan,command);}
uint32_t mt76_rf_bank6_guard_address(unsigned op){return rf_plan_guard_address(&rf_bank6_plan,op);}
size_t mt76_rf_bank6_packet(unsigned op,uint8_t *out,size_t capacity){return rf_plan_packet(&rf_bank6_plan,op,out,capacity);}
bool mt76_rf_bank6_admit(const struct mt76_rf_table *state,unsigned op){return rf_plan_admit(&rf_bank6_plan,state,op);}
enum mt76_mcu_timing_status mt76_rf_bank6_capture(struct mt76_rf_table *state,const struct mt76_mcu_timing_io *io){return rf_plan_capture(&rf_bank6_plan,state,io);}
uint32_t mt76_rf_bank6_address(unsigned i){return rf_plan_address(&rf_bank6_plan,i);}
unsigned mt76_rf_bank6_value(unsigned i){return i<MT76_RF_BANK6_COUNT?rf_bank6_values[i]:UINT32_MAX;}
bool mt76_rf_bank5_exact_profile_valid(const struct mt76_rf_table *s){
    return s && s->complete && s->operations==MT76_RF_BANK5_OPS &&
        rf_plan_state_valid(&rf_bank5_plan,s,MT76_RF_BANK5_OPS) &&
        s->equal==MT76_RF_BANK5_COUNT && !s->mismatch_mask && !s->mismatch_hi;
}

unsigned mt76_rf_vga_sequence(unsigned command){return rf_plan_sequence(&rf_vga_plan,command);}
uint32_t mt76_rf_vga_guard_address(unsigned op){return rf_plan_guard_address(&rf_vga_plan,op);}
size_t mt76_rf_vga_packet(unsigned op,uint8_t *out,size_t capacity){return rf_plan_packet(&rf_vga_plan,op,out,capacity);}
bool mt76_rf_vga_admit(const struct mt76_rf_table *state,unsigned op){return rf_plan_admit(&rf_vga_plan,state,op);}
enum mt76_mcu_timing_status mt76_rf_vga_capture(struct mt76_rf_table *state,const struct mt76_mcu_timing_io *io){return rf_plan_capture(&rf_vga_plan,state,io);}
uint32_t mt76_rf_vga_address(unsigned i){return rf_plan_address(&rf_vga_plan,i);}
unsigned mt76_rf_vga_value(unsigned i){return i<MT76_RF_VGA_COUNT?rf_vga_values[i]:UINT32_MAX;}
bool mt76_rf_bank6_exact_profile_valid(const struct mt76_rf_table *s){
    return s && s->complete && s->operations==MT76_RF_BANK6_OPS &&
        rf_plan_state_valid(&rf_bank6_plan,s,MT76_RF_BANK6_OPS) &&
        s->equal==MT76_RF_BANK6_COUNT && !s->mismatch_mask && !s->mismatch_hi;
}

unsigned mt76_rf_g20_sequence(unsigned command){return rf_plan_sequence(&rf_g20_plan,command);}
uint32_t mt76_rf_g20_guard_address(unsigned op){return rf_plan_guard_address(&rf_g20_plan,op);}
size_t mt76_rf_g20_packet(unsigned op,uint8_t *out,size_t capacity){return rf_plan_packet(&rf_g20_plan,op,out,capacity);}
bool mt76_rf_g20_admit(const struct mt76_rf_table *state,unsigned op){return rf_plan_admit(&rf_g20_plan,state,op);}
enum mt76_mcu_timing_status mt76_rf_g20_capture(struct mt76_rf_table *state,const struct mt76_mcu_timing_io *io){return rf_plan_capture(&rf_g20_plan,state,io);}
uint32_t mt76_rf_g20_address(unsigned i){return rf_plan_address(&rf_g20_plan,i);}
unsigned mt76_rf_g20_value(unsigned i){return i<MT76_RF_G20_COUNT?rf_g20_values[i]:UINT32_MAX;}
bool mt76_rf_vga_exact_profile_valid(const struct mt76_rf_table *s){
    return s && s->complete && s->operations==MT76_RF_VGA_OPS &&
        rf_plan_state_valid(&rf_vga_plan,s,MT76_RF_VGA_OPS) &&
        s->equal==MT76_RF_VGA_COUNT && !s->mismatch_mask && !s->mismatch_hi;
}

unsigned mt76_rf_trim_sequence(unsigned command){return rf_plan_sequence(&rf_trim_plan,command);}
uint32_t mt76_rf_trim_guard_address(unsigned op){return rf_plan_guard_address(&rf_trim_plan,op);}
size_t mt76_rf_trim_packet(unsigned op,uint8_t *out,size_t capacity){return rf_plan_packet(&rf_trim_plan,op,out,capacity);}
bool mt76_rf_trim_admit(const struct mt76_rf_table *state,unsigned op){return rf_plan_admit(&rf_trim_plan,state,op);}
enum mt76_mcu_timing_status mt76_rf_trim_capture(struct mt76_rf_table *state,const struct mt76_mcu_timing_io *io){return rf_plan_capture(&rf_trim_plan,state,io);}
uint32_t mt76_rf_trim_address(unsigned i){return rf_plan_address(&rf_trim_plan,i);}
unsigned mt76_rf_trim_value(unsigned i){return i<MT76_RF_TRIM_COUNT?rf_trim_values[i]:UINT32_MAX;}
bool mt76_rf_g20_exact_profile_valid(const struct mt76_rf_table *s){
    return s && s->complete && s->operations==MT76_RF_G20_OPS &&
        rf_plan_state_valid(&rf_g20_plan,s,MT76_RF_G20_OPS) &&
        s->equal==MT76_RF_G20_COUNT && !s->mismatch_mask && !s->mismatch_hi;
}

bool mt76_rf_trim_exact_profile_valid(const struct mt76_rf_table *s){
    return s && s->complete && s->operations==MT76_RF_TRIM_OPS &&
        rf_plan_state_valid(&rf_trim_plan,s,MT76_RF_TRIM_OPS) &&
        s->equal==1 && !s->mismatch_mask && !s->mismatch_hi;
}
/* RMW each pulse independently, following Linux v7.0 mt76x0_phy_rf_init.
 * No channel or MAC enable commands. Final reads do not certify calibration. */
unsigned mt76_rf_cal_sequence(unsigned q){return q<10?(418+q)%15+1:0;}
bool mt76_rf_cal_write(unsigned q){return q<8 && (q&1);}
uint32_t mt76_rf_cal_address(unsigned q){return q<10?0x80000000u|((q<6 || q==8)?73:4):UINT32_MAX;}
uint32_t mt76_rf_cal_guard_address(unsigned op){
    unsigned n=op%7;return op<70 && n!=3?mt76_rf_probe_read_address(n<3?n:n-4):UINT32_MAX;
}
static unsigned rf_cal_value(const struct mt76_rf_cal *s,unsigned q){
    unsigned v=s->words[q-1][2];return q==3?v&0x7f:v|0x80;
}
static bool rf_cal_state_valid(const struct mt76_rf_cal *s,unsigned op){
    if(!s || !s->attempted || op>70 || s->operations!=op)return false;
    unsigned q=op/7,n=op%7,exchanges=q+(n>3),acks=0;
    if(exchanges>10)return false;
    if(s->reads!=q*6+(n<3?n:3)+(n>4?n-4:0) || s->exchanges!=exchanges || s->received!=(exchanges?16:-1))return false;
    for(unsigned i=0;i<exchanges;i++){
        uint32_t h=0x4a000008u|(mt76_rf_cal_sequence(i)<<16);
        if(s->words[i][0]!=h || s->words[i][1]!=mt76_rf_cal_address(i) || s->words[i][2]>255 || s->words[i][3]!=h)return false;
        if(mt76_rf_cal_write(i)){if(s->words[i][2]!=rf_cal_value(s,i))return false;acks++;}
    }
    if(s->acks!=acks || s->observed!=exchanges-acks)return false;
    for(unsigned i=0;i<10 && i<=q;i++){
        unsigned done=i<q?7:n;
        for(unsigned j=0;j<3;j++){
            if(done>j && !rf_central_guard_valid(j,s->guards[2*i][j]))return false;
            if(done>4+j && !rf_central_guard_valid(j,s->guards[2*i+1][j]))return false;
        }
    }
    return true;
}
bool mt76_rf_cal_admit(const struct mt76_rf_cal *s,unsigned op){return s && !s->complete && op<70 && rf_cal_state_valid(s,op);}
size_t mt76_rf_cal_packet(const struct mt76_rf_cal *s,unsigned op,uint8_t *out,size_t capacity){
    if(!mt76_rf_cal_admit(s,op) || op%7!=3)return 0;
    unsigned q=op/7;uint8_t payload[8]={0};uint32_t address=mt76_rf_cal_address(q);
    for(unsigned b=0;b<4;b++)payload[b]=(uint8_t)(address>>(8*b));
    if(mt76_rf_cal_write(q))payload[4]=(uint8_t)rf_cal_value(s,q);
    return mt76_mcu_encode(mt76_rf_cal_write(q)?12:10,mt76_rf_cal_sequence(q),payload,8,out,capacity);
}
enum mt76_mcu_timing_status mt76_rf_cal_capture(struct mt76_rf_cal *s,const struct mt76_mcu_timing_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->read || !io->exchange)return MT76_TIMING_REFUSED;
    *s=(struct mt76_rf_cal){.attempted=true,.received=-1};
    for(unsigned op=0;op<70;op++){
        if(!mt76_rf_cal_admit(s,op))return MT76_TIMING_STATE;
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        unsigned q=op/7,n=op%7;
        if(n!=3){
            unsigned edge=2*q+(n>=4),j=n<3?n:n-4;uint32_t *v=&s->guards[edge][j];
            if(!io->read(io->context,mt76_rf_cal_guard_address(op),v))return MT76_TIMING_IO;
            s->reads++;s->operations++;
            if(!rf_central_guard_valid(j,*v))return MT76_TIMING_STATE;
        }else{
            uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX];
            if(mt76_rf_cal_packet(s,op,packet,sizeof packet)!=16)return MT76_TIMING_STATE;
            s->received=io->exchange(io->context,packet,sizeof packet,reply,sizeof reply);s->exchanges++;
            for(unsigned j=0;j<4;j++)if(s->received>=(int)(4*j+4))for(unsigned b=0;b<4;b++)s->words[q][j]|=(uint32_t)reply[4*j+b]<<(8*b);
            if(s->received<0)return MT76_TIMING_IO;
            uint32_t h=0x4a000008u|(mt76_rf_cal_sequence(q)<<16);
            if(s->received!=16 || s->words[q][0]!=h || s->words[q][1]!=mt76_rf_cal_address(q) || s->words[q][2]>255 || s->words[q][3]!=h)return MT76_TIMING_REPLY;
            if(mt76_rf_cal_write(q)){if(s->words[q][2]!=rf_cal_value(s,q))return MT76_TIMING_REPLY;s->acks++;}else s->observed++;
            s->operations++;
        }
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    if(!rf_cal_state_valid(s,70))return MT76_TIMING_STATE;
    s->complete=true;return MT76_TIMING_OK;
}

bool mt76_rf_cal_pending_profile_valid(const struct mt76_rf_cal *s){
    static const unsigned values[10]={147,147,147,19,19,147,48,176,147,176};
    if(!s || !s->complete || !rf_cal_state_valid(s,70))return false;
    for(unsigned i=0;i<10;i++)if(s->words[i][2]!=values[i])return false;
    return true;
}
unsigned mt76_rf_wait_sequence(unsigned q){return q<6?(428+q)%15+1:0;}
unsigned mt76_rf_wait_delay(unsigned q){static const unsigned us[6]={10000,40000,50000,100000,300000,500000};return q<6?us[q]:0;}
uint32_t mt76_rf_wait_guard_address(unsigned op){unsigned n=op%7;return op<42 && n!=3?mt76_rf_probe_read_address(n<3?n:n-4):UINT32_MAX;}
static bool rf_wait_state_valid(const struct mt76_rf_wait *s,unsigned op){
    if(!s || !s->attempted || op>42 || s->operations!=op || s->waits>6)return false;
    unsigned q=op/7,n=op%7,ex=q+(n>3),delay=0,first=0;
    if(ex>6 || s->reads!=q*6+(n<3?n:3)+(n>4?n-4:0) || s->exchanges!=ex || s->received!=(ex?16:-1))return false;
    for(unsigned i=0;i<s->waits;i++)delay+=mt76_rf_wait_delay(i);
    if(s->delay_us!=delay)return false;
    for(unsigned i=0;i<ex;i++){
        uint32_t h=0x4a000008u|(mt76_rf_wait_sequence(i)<<16);
        if(s->words[i][0]!=h || s->words[i][1]!=0x80000004u || s->words[i][2]>255 || s->words[i][3]!=h)return false;
        if(!first && !(s->words[i][2]&128))first=i+1;
    }
    if(s->first_clear!=first)return false;
    for(unsigned i=0;i<6 && i<=q;i++){
        unsigned done=i<q?7:n;
        for(unsigned j=0;j<3;j++){
            if(done>j && !rf_central_guard_valid(j,s->guards[2*i][j]))return false;
            if(done>4+j && !rf_central_guard_valid(j,s->guards[2*i+1][j]))return false;
        }
    }
    return true;
}
bool mt76_rf_wait_delay_admit(const struct mt76_rf_wait *s){return s && !s->complete && s->operations<42 && !(s->operations%7) && s->waits==s->operations/7 && rf_wait_state_valid(s,s->operations);}
bool mt76_rf_wait_admit(const struct mt76_rf_wait *s,unsigned op){return s && !s->complete && op<42 && s->waits==op/7+1 && rf_wait_state_valid(s,op);}
size_t mt76_rf_wait_packet(const struct mt76_rf_wait *s,unsigned op,uint8_t *out,size_t capacity){
    if(!mt76_rf_wait_admit(s,op) || op%7!=3)return 0;
    const uint8_t payload[8]={4,0,0,128,0,0,0,0};
    return mt76_mcu_encode(10,mt76_rf_wait_sequence(op/7),payload,8,out,capacity);
}
enum mt76_mcu_timing_status mt76_rf_wait_capture(struct mt76_rf_wait *s,const struct mt76_rf_wait_io *io){
    if(!s || s->attempted || !io || !io->delay || !io->command.authorize || !io->command.read || !io->command.exchange)return MT76_TIMING_REFUSED;
    const struct mt76_mcu_timing_io *c=&io->command;*s=(struct mt76_rf_wait){.attempted=true,.received=-1};
    for(unsigned op=0;op<42;op++){
        unsigned q=op/7,n=op%7;
        if(!n){
            if(!mt76_rf_wait_delay_admit(s))return MT76_TIMING_STATE;
            if(!c->authorize(c->context))return MT76_TIMING_CANCELLED;
            if(!io->delay(c->context,mt76_rf_wait_delay(q)))return MT76_TIMING_IO;
            s->waits++;s->delay_us+=mt76_rf_wait_delay(q);
        }
        if(!mt76_rf_wait_admit(s,op))return MT76_TIMING_STATE;
        if(!c->authorize(c->context))return MT76_TIMING_CANCELLED;
        if(n!=3){
            unsigned edge=2*q+(n>=4),j=n<3?n:n-4;uint32_t *v=&s->guards[edge][j];
            if(!c->read(c->context,mt76_rf_wait_guard_address(op),v))return MT76_TIMING_IO;
            s->reads++;s->operations++;if(!rf_central_guard_valid(j,*v))return MT76_TIMING_STATE;
        }else{
            uint8_t packet[16],reply[MT76_MCU_RESPONSE_MAX];
            if(mt76_rf_wait_packet(s,op,packet,sizeof packet)!=16)return MT76_TIMING_STATE;
            s->received=c->exchange(c->context,packet,16,reply,sizeof reply);s->exchanges++;
            for(unsigned j=0;j<4;j++)if(s->received>=(int)(4*j+4))for(unsigned b=0;b<4;b++)s->words[q][j]|=(uint32_t)reply[4*j+b]<<(8*b);
            if(s->received<0)return MT76_TIMING_IO;
            uint32_t h=0x4a000008u|(mt76_rf_wait_sequence(q)<<16);
            if(s->received!=16 || s->words[q][0]!=h || s->words[q][1]!=0x80000004u || s->words[q][2]>255 || s->words[q][3]!=h)return MT76_TIMING_REPLY;
            if(!s->first_clear && !(s->words[q][2]&128))s->first_clear=q+1;
            s->operations++;
        }
    }
    if(!c->authorize(c->context))return MT76_TIMING_CANCELLED;
    if(s->waits!=6 || !rf_wait_state_valid(s,42))return MT76_TIMING_STATE;
    s->complete=true;return MT76_TIMING_OK;
}

bool mt76_rf_wait_clear_profile_valid(const struct mt76_rf_wait *s){
    if(!s || !s->complete || s->waits!=6 || !rf_wait_state_valid(s,42) || s->first_clear!=1)return false;
    for(unsigned i=0;i<6;i++)if(s->words[i][2]!=48)return false;
    return true;
}
uint32_t mt76_path_address(unsigned i){return i<2?(i?0x2714:0x2300):UINT32_MAX;}
uint32_t mt76_path_value(unsigned i,uint32_t before){return i<2?before&~(i?3u:0x18u):UINT32_MAX;}
bool mt76_path_request(unsigned op,bool *write,uint32_t *address){
    if(op>=18 || !write || !address)return false;
    unsigned n=op%9;*write=n==4;*address=n<3?mt76_rf_probe_read_address(n):n>=6?mt76_rf_probe_read_address(n-6):mt76_path_address(op/9);
    return true;
}
static bool path_state_valid(const struct mt76_path *s,unsigned op){
    if(!s || !s->attempted || s->operations!=op || op>18)return false;
    unsigned writes=0,verified=0;
    for(unsigned i=0;i<2;i++){
        unsigned done=op>=9*(i+1)?9:op>9*i?op-9*i:0;
        for(unsigned j=0;j<3;j++){
            if(done>j && !rf_central_guard_valid(j,s->guards[i*2][j]))return false;
            if(done>6+j && !rf_central_guard_valid(j,s->guards[i*2+1][j]))return false;
        }
        if(done>3 && s->before[i]==UINT32_MAX)return false;
        if(done>4)writes++;
        if(done>5){if(s->after[i]!=mt76_path_value(i,s->before[i]))return false;verified++;}
    }
    return s->writes==writes && s->verified==verified && s->reads==op-writes;
}
bool mt76_path_transfer_valid(const struct mt76_path *s,unsigned op,bool write,uint32_t a,const uint8_t *data,unsigned length){
    bool w;uint32_t address;
    if(!s || s->complete || !data || length!=4 || !mt76_path_request(op,&w,&address) || write!=w || a!=address || !path_state_valid(s,op))return false;
    if(write){uint32_t v=mt76_path_value(op/9,s->before[op/9]);for(unsigned j=0;j<4;j++)if(data[j]!=(uint8_t)(v>>(8*j)))return false;}
    return true;
}
enum mt76_mcu_timing_status mt76_path_apply(struct mt76_path *s,const struct mt76_path_io *io){
    if(!s || s->attempted || !io || !io->authorize || !io->transfer)return MT76_TIMING_REFUSED;
    *s=(struct mt76_path){.attempted=true};
    for(unsigned op=0;op<18;op++){
        bool write;uint32_t a;unsigned i=op/9,n=op%9;uint8_t data[4]={0};
        if(!mt76_path_request(op,&write,&a))return MT76_TIMING_STATE;
        if(write){uint32_t v=mt76_path_value(i,s->before[i]);for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));}
        if(!mt76_path_transfer_valid(s,op,write,a,data,4))return MT76_TIMING_STATE;
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        if(!io->transfer(io->context,op,write,a,data,4))return MT76_TIMING_IO;
        uint32_t v=0;for(unsigned j=0;j<4;j++)v|=(uint32_t)data[j]<<(8*j);
        s->operations++;if(write)s->writes++;else s->reads++;
        if(n<3 || n>=6){unsigned j=n<3?n:n-6;s->guards[i*2+(n>=6)][j]=v;if(!rf_central_guard_valid(j,v))return MT76_TIMING_STATE;}
        else if(n==3){s->before[i]=v;if(v==UINT32_MAX)return MT76_TIMING_STATE;}
        else if(n==5){s->after[i]=v;if(v!=mt76_path_value(i,s->before[i]))return MT76_TIMING_STATE;s->verified++;}
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    if(!path_state_valid(s,18))return MT76_TIMING_STATE;
    s->complete=true;return MT76_TIMING_OK;
}

bool mt76_path_exact_profile_valid(const struct mt76_path *s){
    return s && s->complete && path_state_valid(s,18) && s->before[0]==0x21400 && s->after[0]==0x21400 && !s->before[1] && !s->after[1];
}
/* US_CYC,TXOP,CORE1,AGC0,TXBE0,TX_BAND,EXT_CCA,TX_SW,IBI9,TX_ALC,BCN_TIME,USB_DMA. */
static const uint32_t channel_state_addresses[12]={0x2a4,0x1340,0x2004,0x2300,0x2700,0x132c,0x141c,0x1330,0x2124,0x13b0,0x1114,0x238};
uint32_t mt76_channel_state_address(unsigned i){return i<12?channel_state_addresses[i]:UINT32_MAX;}
uint32_t mt76_channel_state_request(unsigned op){return op<3?mt76_rf_probe_read_address(op):op<15?mt76_channel_state_address(op-3):op<18?mt76_rf_probe_read_address(op-15):UINT32_MAX;}
static bool channel_state_admit(const struct mt76_channel_state *s,bool raw){
    if(!s || s->raw_ibi9!=raw || !s->attempted || s->complete || s->reads>=18)return false;
    for(unsigned op=0;op<s->reads;op++){
        if(op<3 || op>=15){if(!rf_central_guard_valid(op<3?op:op-15,s->guards[op>=15][op<3?op:op-15]))return false;}
        else if(s->values[op-3]==UINT32_MAX && !(raw && op==11))return false;
    }
    return true;
}
static enum mt76_mcu_timing_status channel_state_capture(struct mt76_channel_state *s,const struct mt76_mcu_timing_io *io,bool raw){
    if(!s || s->attempted || !io || !io->authorize || !io->read)return MT76_TIMING_REFUSED;
    *s=(struct mt76_channel_state){.attempted=true,.raw_ibi9=raw};
    for(unsigned op=0;op<18;op++){
        if(!channel_state_admit(s,raw))return MT76_TIMING_STATE;
        if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
        uint32_t *v=op<3?&s->guards[0][op]:op>=15?&s->guards[1][op-15]:&s->values[op-3];
        if(!io->read(io->context,mt76_channel_state_request(op),v))return MT76_TIMING_IO;
        s->reads++;
        if((*v==UINT32_MAX && !(raw && op==11)) || ((op<3 || op>=15) && !rf_central_guard_valid(op<3?op:op-15,*v)))return MT76_TIMING_STATE;
    }
    if(!io->authorize(io->context))return MT76_TIMING_CANCELLED;
    s->complete=true;return MT76_TIMING_OK;
}

bool mt76_channel_state_admit(const struct mt76_channel_state *s){return channel_state_admit(s,false);}
bool mt76_channel_observe_admit(const struct mt76_channel_state *s){return channel_state_admit(s,true);}
unsigned mt76_channel_observe_all_ones(const struct mt76_channel_state *s){
    if(!s || !s->attempted || !s->raw_ibi9 || s->reads>18)return 0;
    unsigned count=s->reads>3?s->reads-3:0,mask=0;if(count>12)count=12;
    for(unsigned i=0;i<count;i++)if(s->values[i]==UINT32_MAX)mask|=1u<<i;
    return mask;
}
enum mt76_mcu_timing_status mt76_channel_state_capture(struct mt76_channel_state *s,const struct mt76_mcu_timing_io *io){return channel_state_capture(s,io,false);}
enum mt76_mcu_timing_status mt76_channel_observe_capture(struct mt76_channel_state *s,const struct mt76_mcu_timing_io *io){return channel_state_capture(s,io,true);}
