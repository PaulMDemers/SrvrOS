#include <srvros/mt76_radio.h>

static bool owned(const struct mt76_radio_io *io) {
    return io->authorize(io->registers.context);
}
static enum mt76_radio_status read_reg(struct mt76_radio_prepare *s,
    const struct mt76_radio_io *io,uint32_t address,uint32_t *value) {
    if(!owned(io))return MT76_RADIO_CANCELLED;
    s->reads++;s->last_address=address;
    if(!io->registers.read(io->registers.context,address,value))return MT76_RADIO_IO;
    s->last_value=*value;
    return MT76_RADIO_OK;
}
static enum mt76_radio_status write_ctrl(struct mt76_radio_prepare *s,
    const struct mt76_radio_io *io,uint32_t value) {
    if(!owned(io))return MT76_RADIO_CANCELLED;
    s->writes++;s->last_address=0x1004;s->last_value=value;
    if(!io->registers.write(io->registers.context,0x1004,value))return MT76_RADIO_IO;
    return MT76_RADIO_OK;
}
static enum mt76_radio_status delay(struct mt76_radio_prepare *s,
    const struct mt76_radio_io *io,uint32_t us) {
    if(!owned(io) || !io->registers.delay(io->registers.context,us))return MT76_RADIO_CANCELLED;
    s->delay_us+=us;return MT76_RADIO_OK;
}
static enum mt76_radio_status idle(struct mt76_radio_prepare *s,
    const struct mt76_radio_io *io,uint32_t address,uint32_t mask) {
    for(unsigned n=0;n<101;n++) {
        uint32_t value;
        enum mt76_radio_status status=read_reg(s,io,address,&value);
        if(status!=MT76_RADIO_OK)return status;
        if(value==UINT32_MAX)return MT76_RADIO_STATE;
        if(!(value&mask))return MT76_RADIO_OK;
        if(n==100)return MT76_RADIO_TIMEOUT;
        status=delay(s,io,1000);
        if(status!=MT76_RADIO_OK)return status;
    }
    return MT76_RADIO_TIMEOUT;
}
#define CHECK(op) do { enum mt76_radio_status status=(op); if(status!=MT76_RADIO_OK)return status; } while(0)
enum mt76_radio_status mt76_radio_prepare_reset(struct mt76_radio_prepare *s,
    const struct mt76_radio_io *io,const struct mt76_identity *identity) {
    if(!s || s->attempted || !io || !identity || !io->authorize ||
        !io->registers.read || !io->registers.write || !io->registers.delay ||
        identity->asic!=0x76100002 || identity->mac!=0x76502000)return MT76_RADIO_REFUSED;
    s->attempted=true;
    uint32_t value;
    CHECK(read_reg(s,io,0x1000,&value));
    if(value!=identity->mac)return MT76_RADIO_STATE;
    CHECK(read_reg(s,io,0x1004,&value));
    if(value!=0x20)return MT76_RADIO_STATE; /* Native post-upload disabled state. */
    CHECK(idle(s,io,0x208,0x0a)); /* WPDMA TX/RX busy. */
    CHECK(idle(s,io,0x1200,3));   /* MAC TX/RX busy. */
    /* Recheck immediately before mutation; exclusive ownership remains required. */
    CHECK(read_reg(s,io,0x1004,&value));
    if(value!=0x20)return MT76_RADIO_STATE;
    s->reset_attempted=true;
    CHECK(write_ctrl(s,io,3)); /* RESET_CSR | RESET_BBP, TX/RX clear. */
    for(unsigned n=0;n<40;n++)CHECK(delay(s,io,5000));
    CHECK(read_reg(s,io,0x1004,&value));
    if(value&~3u)return MT76_RADIO_STATE;
    CHECK(write_ctrl(s,io,0));
    CHECK(read_reg(s,io,0x1004,&value));
    if(value)return MT76_RADIO_STATE;
    s->reset_released=true;
    CHECK(idle(s,io,0x208,0x0a));
    CHECK(idle(s,io,0x1200,3));
    s->prepared=true;
    return MT76_RADIO_OK;
}
