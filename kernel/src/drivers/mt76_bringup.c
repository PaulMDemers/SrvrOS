#include <srvros/mt76_bringup.h>

#define EFUSE_CTRL 0x24u
#define EFUSE_DATA 0x28u
#define EFUSE_KICK (1u << 30)
#define EFUSE_ADDRESS (0x3ffu << 16)
#define EFUSE_MODE (3u << 6)
#define EFUSE_POLL_BUDGET 1024u

static void clear_capture(struct mt76_efuse_capture *out) {
    for (size_t i = 0; i < sizeof out->bytes; ++i) out->bytes[i] = 0;
    out->unmapped_blocks = 0;
}

enum mt76_capture_status mt76_capture_efuse(const struct mt76_register_io *io,
    const struct mt76_identity *identity, struct mt76_efuse_capture *out) {
    if (!out) return MT76_CAPTURE_REFUSED;
    clear_capture(out);
    if (!io || !io->read || !io->write || !io->delay || !identity ||
        identity->asic != 0x76100002 || identity->mac != 0x76502000) return MT76_CAPTURE_REFUSED;
    enum mt76_capture_status failure = MT76_CAPTURE_IO;
    unsigned budget = EFUSE_POLL_BUDGET;
    bool any_data = false;
    for (unsigned block = 0; block < MT76_EFUSE_BYTES/16; ++block) {
        if (!budget) { failure = MT76_CAPTURE_TIMEOUT; goto fail; }
        uint32_t value;
        if (!io->read(io->context, EFUSE_CTRL, &value)) goto fail;
        if (value & EFUSE_KICK) { failure = MT76_CAPTURE_BUSY; goto fail; }
        /* Preserve timing/selection, remove previous address and all mode bits.
         * Mode zero requests a logical read; never generate programming modes. */
        value = (value & ~(EFUSE_ADDRESS | EFUSE_MODE)) | (block*16u << 16) | EFUSE_KICK;
        if (!io->write(io->context, EFUSE_CTRL, value)) goto fail;
        for (;;) {
            if (!budget) { failure = MT76_CAPTURE_TIMEOUT; goto fail; }
            --budget;
            if (!io->read(io->context, EFUSE_CTRL, &value)) goto fail;
            if (!(value & EFUSE_KICK)) break;
            if (!io->delay(io->context, 1000)) { failure = MT76_CAPTURE_CANCELLED; goto fail; }
        }
        if (!io->delay(io->context, 2)) { failure = MT76_CAPTURE_CANCELLED; goto fail; }
        if (!io->read(io->context, EFUSE_CTRL, &value)) goto fail;
        if (value & EFUSE_KICK) { failure = MT76_CAPTURE_BUSY; goto fail; }
        if ((value & 63u) == 63u) {
            out->unmapped_blocks |= 1u << block;
            for (unsigned i = 0; i < 16; ++i) out->bytes[block*16+i] = 0xff;
            continue;
        }
        for (unsigned word = 0; word < 4; ++word) {
            if (!io->read(io->context, EFUSE_DATA+word*4, &value)) goto fail;
            for (unsigned byte = 0; byte < 4; ++byte) {
                uint8_t v = (uint8_t)(value >> (byte*8));
                out->bytes[block*16+word*4+byte] = v;
                if (v != 0xff && v != 0) any_data = true;
            }
        }
    }
    if (!any_data) { failure = MT76_CAPTURE_EMPTY; goto fail; }
    return MT76_CAPTURE_OK;
fail:
    clear_capture(out);
    return failure;
}

bool mt76_efuse_map_command(uint32_t previous,unsigned block,uint32_t *command){
    if(!command || block>=2 || previous==UINT32_MAX || (previous&EFUSE_KICK))return false;
    *command=(previous&~(EFUSE_ADDRESS|EFUSE_MODE))|((0x1e0u+16u*block)<<16)|(1u<<6)|EFUSE_KICK;
    return true;
}
bool mt76_efuse_map_analyze(const uint8_t *bytes,size_t length,struct mt76_efuse_map *s){
    if(!bytes || length!=32 || !s)return false;
    s->zero_count=s->first_zero=s->last_zero=s->free_span=s->reference_used=0;s->valid=false;
    bool meaningful=false;
    for(unsigned i=0;i<29;i++){
        if(bytes[i]!=0 && bytes[i]!=0xff)meaningful=true;
        if(!bytes[i]){if(!s->zero_count)s->first_zero=0x1e0+i;s->last_zero=0x1e0+i;s->zero_count++;}
    }
    /* Match upstream span arithmetic, including no-zero => span1. This is
     * not a count of actual used entries. Padding bytes29..31 are excluded. */
    s->free_span=s->last_zero-s->first_zero+1;
    s->reference_used=29-s->free_span;
    /* Reject unavailable/all-ff evidence even though span arithmetic alone
     * would pass it. A native refusal must remain visible, not relaxed. */
    s->valid=meaningful && s->reference_used>=5;
    return s->valid;
}
enum mt76_capture_status mt76_capture_efuse_map(const struct mt76_efuse_map_io *io,
    const struct mt76_identity *identity,struct mt76_efuse_map *s){
    if(!s || s->attempted || !io || !io->authorize || !io->registers.read || !io->registers.write ||
       !io->registers.delay || !identity || identity->asic!=0x76100002 || identity->mac!=0x76502000)return MT76_CAPTURE_REFUSED;
    *s=(struct mt76_efuse_map){.attempted=true};uint8_t data[32]={0};enum mt76_capture_status result=MT76_CAPTURE_IO;
    const struct mt76_register_io *r=&io->registers;uint32_t v,command;
#define MAP_AUTH() do{if(!io->authorize(r->context)){result=MT76_CAPTURE_CANCELLED;goto done;}}while(0)
#define MAP_READ(a) do{MAP_AUTH();if(!r->read(r->context,(a),&v))goto done;s->reads++;}while(0)
#define MAP_GUARDS() do{MAP_READ(0x1004);if(v){result=MT76_CAPTURE_BUSY;goto done;}MAP_READ(0x2000);if(v!=0xf000f200){result=MT76_CAPTURE_BUSY;goto done;}}while(0)
    for(unsigned block=0;block<2;block++){
        MAP_GUARDS();MAP_READ(0x208);if(v&15){result=MT76_CAPTURE_BUSY;goto done;}
        MAP_READ(EFUSE_CTRL);
        if(!mt76_efuse_map_command(v,block,&command)){result=MT76_CAPTURE_BUSY;goto done;}
        MAP_AUTH();if(!r->write(r->context,EFUSE_CTRL,command))goto done;s->writes++;
        for(;;){
            if(s->polls==EFUSE_POLL_BUDGET){result=MT76_CAPTURE_TIMEOUT;goto done;}
            s->polls++;MAP_READ(EFUSE_CTRL);if(v==UINT32_MAX){result=MT76_CAPTURE_BUSY;goto done;}if(!(v&EFUSE_KICK))break;
            MAP_AUTH();if(!r->delay(r->context,1000)){result=MT76_CAPTURE_CANCELLED;goto done;}
        }
        MAP_AUTH();if(!r->delay(r->context,2)){result=MT76_CAPTURE_CANCELLED;goto done;}
        MAP_READ(EFUSE_CTRL);
        if(v==UINT32_MAX || v&EFUSE_KICK || (v&EFUSE_MODE)!=(1u<<6) || (v&EFUSE_ADDRESS)!=(command&EFUSE_ADDRESS)){
            result=MT76_CAPTURE_BUSY;goto done;
        }
        if((v&63)==63){result=MT76_CAPTURE_EMPTY;goto done;}
        for(unsigned word=0;word<4;word++){MAP_READ(EFUSE_DATA+word*4);for(unsigned byte=0;byte<4;byte++)data[block*16+word*4+byte]=(uint8_t)(v>>(8*byte));}
        MAP_GUARDS();s->blocks++;
    }
    MAP_AUTH();s->complete=true;
    result=mt76_efuse_map_analyze(data,sizeof data,s)?MT76_CAPTURE_OK:MT76_CAPTURE_EMPTY;
done:
    for(unsigned i=0;i<sizeof data;i++)((volatile uint8_t *)data)[i]=0;
    return result;
#undef MAP_AUTH
#undef MAP_READ
#undef MAP_GUARDS
}

bool mt76_firmware_chunk(const uint8_t *image, size_t size, size_t index,
    struct mt76_fw_chunk *out) {
    if (!out) return false;
    *out = (struct mt76_fw_chunk){0};
    struct mt76_firmware_layout layout;
    if (size > 1024*1024 || !mt76_firmware_layout(image, size, &layout)) return false;
    /* Do not let the ILM destination overlap the DLM address window. */
    if (layout.ilm_size > 0x80000-64 || layout.dlm_size > 0x80000) return false;
    const size_t limit = 0x38f0; /* 0x38f8 packet budget less header/trailer. */
    size_t ilm_count = (layout.ilm_size+limit-1)/limit;
    size_t dlm_count = (layout.dlm_size+limit-1)/limit;
    if (index > ilm_count+dlm_count) return false;
    if (index == ilm_count+dlm_count) {
        *out = (struct mt76_fw_chunk){MT76_FW_IVB,32,64,64,64,0};
        return true;
    }
    bool ilm = index < ilm_count;
    size_t pos = (ilm ? index : index-ilm_count)*limit;
    size_t count = (ilm ? layout.ilm_size : layout.dlm_size)-pos;
    if (count > limit) count = limit;
    size_t padded = (count+3)&~(size_t)3;
    *out = (struct mt76_fw_chunk){ilm ? MT76_FW_ILM : MT76_FW_DLM,
        (ilm ? layout.ilm_offset : layout.dlm_offset)+pos,count,padded,padded+8,
        (uint32_t)((ilm ? 64 : 0x80000)+pos)};
    return true;
}

static bool firmware_overlap(const void *a, size_t na, const void *b, size_t nb) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<na : x-y<nb;
}

size_t mt76_firmware_packet(const uint8_t *image, size_t size, size_t index,
    uint8_t *packet, size_t capacity) {
    struct mt76_fw_chunk chunk;
    if (!packet || !mt76_firmware_chunk(image,size,index,&chunk) ||
        chunk.segment == MT76_FW_IVB || capacity < chunk.wire_bytes ||
        firmware_overlap(image,size,packet,chunk.wire_bytes)) return 0;
    /* CPU_TX_PORT=2, command type, unpadded payload length. */
    uint32_t header = (1u<<30) | (2u<<27) | (uint32_t)chunk.bytes;
    for (unsigned i=0;i<4;i++) packet[i]=(uint8_t)(header>>(8*i));
    for (size_t i=0;i<chunk.bytes;i++) packet[4+i]=image[chunk.file_offset+i];
    for (size_t i=4+chunk.bytes;i<chunk.wire_bytes;i++) packet[i]=0;
    return chunk.wire_bytes;
}

enum mt76_upload_status mt76_upload_firmware(struct mt76_firmware_session *session,
    const struct mt76_firmware_io *io, const struct mt76_identity *identity,
    const uint8_t *image, size_t size, uint8_t *scratch, size_t capacity) {
    struct mt76_fw_chunk chunk;
    if (!session || session->attempted || !io || !io->authorize || !io->fce_write ||
        !io->read || !io->write || !io->bulk || !io->activate || !io->delay ||
        !identity || identity->asic!=0x76100002 || identity->mac!=0x76502000 ||
        !scratch || capacity<MT76_FW_PACKET_MAX || !mt76_firmware_chunk(image,size,0,&chunk) ||
        firmware_overlap(image,size,scratch,MT76_FW_PACKET_MAX))
        return MT76_UPLOAD_REFUSED;
    session->attempted=true;
    if (!io->authorize(io->context,image,size,identity)) return MT76_UPLOAD_REFUSED;
    for (size_t index=0;;index++) {
        if (!mt76_firmware_chunk(image,size,index,&chunk)) return MT76_UPLOAD_REFUSED;
        if (chunk.segment==MT76_FW_IVB) {
            if (io->activate(io->context,image+chunk.file_offset,chunk.bytes)!=(int)chunk.bytes)
                return MT76_UPLOAD_IO;
            break;
        }
        size_t length=mt76_firmware_packet(image,size,index,scratch,capacity);
        if (!length) return MT76_UPLOAD_REFUSED;
        if (!io->fce_write(io->context,0x230,chunk.destination) ||
            !io->fce_write(io->context,0x234,(uint32_t)chunk.padded_bytes<<16) ||
            io->bulk(io->context,scratch,length)!=(int)length) return MT76_UPLOAD_IO;
        uint32_t descriptor;
        if (!io->read(io->context,0x9a8,&descriptor) ||
            !io->write(io->context,0x9a8,descriptor+1)) return MT76_UPLOAD_IO;
        if (!io->delay(io->context,5000)) return MT76_UPLOAD_CANCELLED;
    }
    for (unsigned poll=0;poll<1000;poll++) {
        uint32_t ready;
        if (!io->read(io->context,0x730,&ready)) return MT76_UPLOAD_IO;
        /* All-ones is not proof of a live MCU. */
        if (ready!=0xffffffffu && (ready&1)) return MT76_UPLOAD_OK;
        if (!io->delay(io->context,1000)) return MT76_UPLOAD_CANCELLED;
    }
    return MT76_UPLOAD_TIMEOUT;
}

enum mt76_upload_status mt76_prepare_firmware(struct mt76_prepare_session *session,
    const struct mt76_prepare_io *io, const struct mt76_identity *identity) {
    if (!session || session->attempted || !io || !io->reset ||
        !io->registers.read || !io->registers.write || !io->registers.delay ||
        !identity || identity->asic!=0x76100002 || identity->mac!=0x76502000)
        return MT76_UPLOAD_REFUSED;
    session->attempted=true;session->prepared=false;
    const struct mt76_register_io *r=&io->registers;
    uint32_t value;
    /* Refuse an already running or unreadable MCU instead of taking it over. */
    if (!r->read(r->context,0x730,&value)) return MT76_UPLOAD_IO;
    if (value==0xffffffffu || (value&1)) return MT76_UPLOAD_REFUSED;
    /* Linux v6.18 mt76x0_set_wlan_state enables the WLAN block and clock
     * before firmware reset. Preserve board GPIO/antenna selection; do not
     * replay its optional RF/GPIO reset sequence. MAC TX/RX stays disabled. */
    if (!r->read(r->context,0x80,&value)) return MT76_UPLOAD_IO;
    if (value==0xffffffffu || (value&0xcu)) return MT76_UPLOAD_REFUSED;
    uint32_t enabled=value|3u;
    if (!r->write(r->context,0x80,enabled)) return MT76_UPLOAD_IO;
    if (!r->delay(r->context,20)) return MT76_UPLOAD_CANCELLED;
    if (!r->read(r->context,0x80,&value)) return MT76_UPLOAD_IO;
    /* GPIO input bits can change independently of this write. */
    if (value==0xffffffffu || ((value^enabled)&~0xff00u)) return MT76_UPLOAD_REFUSED;
    bool clock_ready=false;
    for (unsigned poll=0;poll<100;poll++) {
        if (!r->read(r->context,0x20,&value)) return MT76_UPLOAD_IO;
        if (value==0xffffffffu) return MT76_UPLOAD_REFUSED;
        if ((value&0xc00000u)==0xc00000u) { clock_ready=true;break; }
        if (!r->delay(r->context,20)) return MT76_UPLOAD_CANCELLED;
    }
    if (!clock_ready) return MT76_UPLOAD_TIMEOUT;
    if (!r->read(r->context,0x238,&value)) return MT76_UPLOAD_IO;
    if (value==0xffffffffu || (value&0xc0000000u)) return MT76_UPLOAD_REFUSED;
    uint32_t startup_mac=session->mac_startup?0x2cu:0x20u;
    if (!r->write(r->context,0x238,0x00c00000) ||
        !r->write(r->context,0x1004,startup_mac) ||
        !r->read(r->context,0x1004,&value)) return MT76_UPLOAD_IO;
    /* v1 keeps TX/RX disabled. Explicit v2 tests the upstream startup value;
     * restore disabled MAC before any upload. Never retry an uncertain write. */
    if (value!=startup_mac) return MT76_UPLOAD_REFUSED;
    if (
        !r->read(r->context,0x238,&value)) return MT76_UPLOAD_IO;
    if (value==0xffffffffu || (value&0xc0000000u)) return MT76_UPLOAD_REFUSED;
    if (!r->write(r->context,0x238,value|0x00c00020) ||
        !io->reset(r->context)) return MT76_UPLOAD_IO;
    if (!r->delay(r->context,5000)) return MT76_UPLOAD_CANCELLED;
    static const uint32_t setup[][2] = {
        {0x800,1}, {0x9a0,0x400230}, {0x9a4,1}, {0x9c4,0x44}, {0xa6c,3}
    };
    for (size_t i=0;i<sizeof setup/sizeof setup[0];i++)
        if (!r->write(r->context,setup[i][0],setup[i][1])) return MT76_UPLOAD_IO;
    if (!r->read(r->context,0x238,&value)) return MT76_UPLOAD_IO;
    if (value==0xffffffffu || (value&0xc0000000u)) return MT76_UPLOAD_REFUSED;
    if (!r->write(r->context,0x238,value|(1u<<16)) ||
        !r->write(r->context,0x238,value&~(1u<<16))) return MT76_UPLOAD_IO;
    if (session->mac_startup && !r->write(r->context,0x1004,0x20)) return MT76_UPLOAD_IO;
    if (!r->read(r->context,0x1004,&value)) return MT76_UPLOAD_IO;
    if (value!=0x20) return MT76_UPLOAD_REFUSED;
    session->prepared=true;
    return MT76_UPLOAD_OK;
}
