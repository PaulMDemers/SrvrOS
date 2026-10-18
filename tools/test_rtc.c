#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t cmos[128];
static unsigned reads, rollover_at;
static uint8_t test_rtc_read(uint8_t index) {
    assert(index < 128 && index != 0x0c);
    if (++reads == rollover_at) cmos[0]++;
    return cmos[index];
}
#define SRVROS_RTC_HOST_TEST
#include "../kernel/src/arch/x86_64/rtc.c"
static uint8_t fadt[244];
static struct acpi_table_info info;
static uint64_t mono, mono_step;
uint64_t acpi_table_count(void) { return 1; }
const struct acpi_table_info *acpi_table_at(uint64_t i) { assert(i==0); return &info; }
void *pmm_phys_to_virt(uint64_t address) { (void)address; return fadt; }
void console_printf(const char *fmt, ...) { (void)fmt; }
void console_write(const char *text) { (void)text; }
uint64_t timer_monotonic_us(void) { uint64_t old=mono; mono+=mono_step; return old; }
static struct rtc_sample sample = { .second=0x56,.minute=0x34,.hour=0x12,
    .day=0x18,.month=0x10,.year=0x26,.century=0x20,.mode=2,.valid=0x80 };
static uint64_t decode(struct rtc_sample s, bool century) {
    uint64_t epoch=0; assert(rtc_decode(&s,century,&epoch)); return epoch;
}
static void invalid(struct rtc_sample s) { uint64_t epoch=123; assert(!rtc_decode(&s,true,&epoch) && epoch==123); }
int main(void) {
    struct rtc_sample s=sample;
    uint64_t expected=1792326896; /* 2026-10-18 12:34:56 UTC, independent fixture. */
    assert(decode(s,true)==expected && decode(s,false)==expected);
    s.mode=0; assert(decode(s,true)==expected-43200);
    s.hour=0x92; assert(decode(s,true)==expected);
    s.hour=0x81; assert(decode(s,true)==expected+3600);
    s=sample;s.mode=6;s.second=56;s.minute=34;s.hour=12;s.day=18;s.month=10;s.year=26;s.century=20;
    assert(decode(s,true)==expected);
    s=sample;s.year=0x70;s.century=0x19;s.month=1;s.day=1;s.hour=s.minute=s.second=0;
    assert(decode(s,true)==0 && decode(s,false)==0);
    s.year=0;s.century=0x20;s.month=2;s.day=0x29;
    assert(decode(s,true)==951782400);
    s.century=0x21;invalid(s);s.century=0x20;s.year=1;invalid(s);
    s=sample;s.month=0;invalid(s);s=sample;s.month=0x13;invalid(s);
    s=sample;s.day=0;invalid(s);s=sample;s.month=4;s.day=0x31;invalid(s);
    s=sample;s.second=0x5a;invalid(s);s=sample;s.second=0x60;invalid(s);
    s=sample;s.hour=0x24;invalid(s);s=sample;s.hour=0x80;invalid(s);
    s=sample;s.mode=0;s.hour=0;invalid(s);s=sample;s.valid=0;invalid(s);
    s=sample;s.mode|=0x80;invalid(s);s=sample;s.century=0;invalid(s);
    uint8_t century=99;
    assert(rtc_fadt(fadt,108,&century) && century==0);
    fadt[108]=0x32;assert(rtc_fadt(fadt,109,&century) && century==0x32);
    fadt[108]=0x80;assert(rtc_fadt(fadt,244,&century) && century==0);
    fadt[108]=0x0c;assert(rtc_fadt(fadt,244,&century) && century==0);
    fadt[108]=0x32;fadt[109]=0x20;assert(!rtc_fadt(fadt,111,&century));fadt[109]=0;
    memcpy(info.signature,"FACP",4);info.length=sizeof(fadt);
    cmos[0]=sample.second;cmos[2]=sample.minute;cmos[4]=sample.hour;cmos[7]=sample.day;
    cmos[8]=sample.month;cmos[9]=sample.year;cmos[0xb]=sample.mode;cmos[0xd]=sample.valid;cmos[0x32]=sample.century;
    assert(timer_realtime_us()==-1);
    mono=500;mono_step=1;assert(timer_wall_init());
    mono_step=0;mono=wall_base_us+987654;
    assert(timer_realtime_us()==(int64_t)(expected*1000000+987654));
    unsigned sampled_reads=reads;mono+=1000000;assert(timer_realtime_us()>0 && reads==sampled_reads);
    mono=wall_base_us-1;assert(timer_realtime_us()==-1);
    mono=500;mono_step=1;reads=0;rollover_at=15;assert(timer_wall_init());
    assert(wall_epoch_us==(expected+1)*1000000 && reads>24);rollover_at=0;
    cmos[0xa]=0x80;mono_step=1000;reads=0;assert(!timer_wall_init() && reads<1000);
    mono_step=0;reads=0;assert(!timer_wall_init() && reads==250000);
    cmos[0xa]=0;fadt[109]=0x20;reads=0;assert(!timer_wall_init() && reads==0);
    assert(timer_realtime_us()==-1);
    puts("rtc-host: PASS (calendar modes, leap days, century, stable sampling, unavailable hardware, bounded waits, cached UTC)");
}
