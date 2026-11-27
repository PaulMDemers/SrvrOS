#include <srvros/backlight.h>
static struct backlight_io device;
static bool available;
static uint32_t period;
static bool cpu_pwm;
static struct backlight_trial trial;
static bool sample(uint32_t *raw) {
    if(!available) return false;
    uint32_t cpu=device.read(device.context,0),control=device.read(device.context,1);
    uint32_t pch=device.read(device.context,2);
    *raw=cpu_pwm?device.read(device.context,3):pch;
    /* Check the captured route and period on every operation. */
    if((cpu_pwm && (cpu!=0x80000000u || control!=0x80000000u || (pch&65535) || (*raw>>16))) ||
       (!cpu_pwm && (cpu || control!=0xc0000000u)) || (pch>>16)!=period ||
       !(*raw&65535) || (*raw&65535)>period) {
        available=false;return false;
    }
    return true;
}
static bool write_verified(uint32_t value) {
    device.write_duty(device.context,value);
    uint32_t readback;
    if(sample(&readback) && readback==value) return true;
    available=false;
    return false;
}
bool backlight_bind_profile(struct backlight_io io,enum backlight_profile profile) {
    available=false;
    trial=(struct backlight_trial){0};
    if(!io.read || !io.write_duty) return false;
    switch(profile) {
    case BACKLIGHT_AIR7_2: case BACKLIGHT_AIR6_1: period=2777;break;
    case BACKLIGHT_PRO11_2: period=1953;break;
    case BACKLIGHT_PRO8_1: period=1808;break;
    default: return false;
    }
    cpu_pwm=profile==BACKLIGHT_PRO8_1;
    device=io;available=true;uint32_t raw;
    if(!sample(&raw)) return false;
    if(cpu_pwm) trial.initial_duty=raw;
    return true;
}
int backlight_percent(unsigned value) {
    if(value && (value<5 || value>100)) return -1;
    uint32_t raw;if(!sample(&raw)) return -1;
    if(value) {
        uint32_t next=(raw&0xffff0000u)|((period*value+50)/100);
        if(next!=raw) {
            if(!write_verified(next)) return -1;
            if(cpu_pwm) {
                trial.writes++;
                if(value<=50) trial.low=true;
                if(value>=60) trial.high=true;
            }
            raw=next;
        }
    }
    return (int)(((raw&65535)*100+period/2)/period);
}

struct backlight_trial backlight_pro81_finish(void) {
    if(!cpu_pwm) return (struct backlight_trial){0};
    uint32_t raw;
    if(sample(&raw)) {
        trial.final_duty=raw;
        trial.restored=raw==trial.initial_duty || write_verified(trial.initial_duty);
    }
    available=false;
    return trial;
}

bool backlight_bind(struct backlight_io io) { return backlight_bind_profile(io,BACKLIGHT_AIR7_2); }
