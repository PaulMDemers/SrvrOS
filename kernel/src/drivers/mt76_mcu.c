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
