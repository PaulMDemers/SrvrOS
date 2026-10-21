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
    if (!r->read(r->context,0x238,&value)) return MT76_UPLOAD_IO;
    if (value==0xffffffffu || (value&0xc0000000u)) return MT76_UPLOAD_REFUSED;
    if (!r->write(r->context,0x238,0x00c00000) ||
        !r->write(r->context,0x1004,0x20) ||
        !r->read(r->context,0x1004,&value)) return MT76_UPLOAD_IO;
    /* Upstream uses 0x2c, which enables MAC TX/RX. This diagnostic deliberately
     * holds both disabled; firmware compatibility must be tested natively. */
    if (value!=0x20) return MT76_UPLOAD_REFUSED;
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
    if (!r->read(r->context,0x1004,&value)) return MT76_UPLOAD_IO;
    if (value!=0x20) return MT76_UPLOAD_REFUSED;
    session->prepared=true;
    return MT76_UPLOAD_OK;
}
