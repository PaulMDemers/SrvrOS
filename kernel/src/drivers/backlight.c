#include <srvros/backlight.h>
static struct backlight_io device;
static bool available;
static uint32_t period;
static bool sample(uint32_t *raw) {
    if(!available) return false;
    uint32_t cpu=device.read(device.context,0),control=device.read(device.context,1);
    *raw=device.read(device.context,2);
    /* This backend supports only the exact PCH override mode observed on the
     * target. Never switch modes or change enable, polarity, period or flags. */
    if(cpu || control!=0xc0000000u || (*raw>>16)!=period || !(*raw&65535) || (*raw&65535)>period) {
        available=false;return false;
    }
    return true;
}
bool backlight_bind_profile(struct backlight_io io,enum backlight_profile profile) {
    available=false;
    if(!io.read || !io.write_duty) return false;
    if(profile!=BACKLIGHT_AIR7_2 && profile!=BACKLIGHT_PRO11_2 && profile!=BACKLIGHT_AIR6_1) return false;
    device=io;period=profile==BACKLIGHT_PRO11_2?1953:2777;available=true;uint32_t raw;
    return sample(&raw); /* Discovery never writes. */
}
int backlight_percent(unsigned value) {
    if(value && (value<5 || value>100)) return -1;
    uint32_t raw;if(!sample(&raw)) return -1;
    if(value) {
        uint32_t next=(raw&0xffff0000u)|((period*value+50)/100);
        if(next!=raw) {
            device.write_duty(device.context,next);
            uint32_t readback;
            if(!sample(&readback) || readback!=next) { available=false;return -1; }
            raw=readback;
        }
    }
    return (int)(((raw&65535)*100+period/2)/period);
}

bool backlight_bind(struct backlight_io io) { return backlight_bind_profile(io,BACKLIGHT_AIR7_2); }
