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
