#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/mt76_probe.h>

struct fake { unsigned calls, fail_at; int result; uint32_t value; };
static int read_register(void *context, const uint8_t setup[8], uint8_t data[4]) {
    struct fake *f = context;
    assert(f->calls < 2);
    const uint8_t expected[8] = {0xc0,7,0,0,0,f->calls ? 0x10 : 0,4,0};
    assert(memcmp(setup, expected, 8) == 0);
    ++f->calls;
    uint32_t value = f->calls == f->fail_at ? f->value : (f->calls == 1 ? 0x76100002 : 0x76502000);
    for (unsigned i = 0; i < 4; ++i) data[i] = (uint8_t)(value >> (8*i));
    return f->calls == f->fail_at ? f->result : 4;
}
static void identity(void) {
    struct mt76_identity id = {1,1};
    struct fake f = {0};
    assert(mt76_probe_identity(read_register, &f, &id));
    assert(f.calls == 2 && id.asic == 0x76100002 && id.mac == 0x76502000);
    for (unsigned at = 1; at <= 2; ++at) {
        for (int result = -2; result <= 5; ++result) {
            if (result == 4) continue;
            f = (struct fake){0,at,result,0x76100001};
            id = (struct mt76_identity){1,1};
            assert(!mt76_probe_identity(read_register, &f, &id));
            assert(f.calls == at && !id.asic && !id.mac);
        }
        for (unsigned bad = 0; bad < 2; ++bad) {
            f = (struct fake){0,at,4,bad ? UINT32_MAX : 0};
            assert(!mt76_probe_identity(read_register, &f, &id));
            assert(f.calls == at && !id.asic && !id.mac);
        }
    }
    assert(!mt76_probe_identity(NULL, NULL, &id));
    assert(!mt76_probe_identity(read_register, &f, NULL));
}
static void firmware(void) {
    uint8_t data[112] = {80,0,0,0,0,0,0,0,0x34,0x12,0x78,0x56};
    struct mt76_firmware_layout layout;
    assert(mt76_firmware_layout(data, sizeof data, &layout));
    assert(layout.ivb_offset == 32 && layout.ivb_size == 64);
    assert(layout.ilm_offset == 96 && layout.ilm_size == 16);
    assert(layout.dlm_offset == 112 && layout.dlm_size == 0);
    assert(layout.build_version == 0x1234 && layout.firmware_version == 0x5678);
    data[0] = 65; data[4] = 15;
    assert(mt76_firmware_layout(data, sizeof data, &layout));
    assert(layout.ilm_size == 1 && layout.dlm_offset == 97 && layout.dlm_size == 15);
    for (size_t n = 0; n < sizeof data; ++n) {
        memset(&layout, 0xff, sizeof layout);
        assert(!mt76_firmware_layout(data, n, &layout));
        assert(!layout.ivb_size && !layout.ilm_size && !layout.dlm_size);
    }
    assert(!mt76_firmware_layout(data, sizeof data + 1, &layout));
    data[0] = 64; data[4] = 16;
    assert(!mt76_firmware_layout(data, sizeof data, &layout));
    memset(data, 0xff, 8);
    assert(!mt76_firmware_layout(data, sizeof data, &layout));
    assert(!mt76_firmware_layout(NULL, sizeof data, &layout));
    assert(!mt76_firmware_layout(data, sizeof data, NULL));
}
int main(void) {
    identity(); firmware();
    puts("MT76 probe contract: PASS (exact reads, early failure, firmware boundaries)");
}
