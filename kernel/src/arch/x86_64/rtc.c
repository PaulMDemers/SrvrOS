/* Read-only UTC wall-clock seed from the legacy CMOS RTC. ACPI 6.6 FADT
 * supplies the optional century register and the "CMOS RTC Not Present" bit.
 * Runtime reads use RAM plus the monotonic clock, never CMOS transactions. */
#include <srvros/acpi.h>
#include <srvros/console.h>
#include <srvros/pmm.h>
#include <srvros/timer.h>
#include "io.h"
#include <stddef.h>

struct rtc_sample { uint8_t second, minute, hour, day, month, year, century, mode, valid; };
static bool wall_ready;
static uint64_t wall_epoch_us, wall_base_us;

static bool rtc_leap(unsigned year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
static int rtc_number(uint8_t value, bool binary) {
    if (binary) return value;
    if ((value & 15) > 9 || (value >> 4) > 9) return -1;
    return (value >> 4) * 10 + (value & 15);
}
static bool rtc_decode(const struct rtc_sample *s, bool century_present, uint64_t *epoch) {
    static const uint8_t month_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (!(s->valid & 0x80) || (s->mode & 0x80)) return false;
    bool binary = (s->mode & 4) != 0, mode24 = (s->mode & 2) != 0;
    int second = rtc_number(s->second,binary), minute = rtc_number(s->minute,binary);
    int hour = rtc_number(mode24 ? s->hour : (s->hour & 0x7f),binary);
    int day = rtc_number(s->day,binary), month = rtc_number(s->month,binary);
    int year = rtc_number(s->year,binary), century = rtc_number(s->century,binary);
    if (second < 0 || second > 59 || minute < 0 || minute > 59 ||
        year < 0 || year > 99 || month < 1 || month > 12 || day < 1) return false;
    if (mode24) { if (hour < 0 || hour > 23) return false; }
    else {
        if (hour < 1 || hour > 12) return false;
        hour = hour % 12 + ((s->hour & 0x80) ? 12 : 0);
    }
    if (century_present) {
        if (century < 19 || century > 99) return false;
        year += century * 100;
    } else {
        /* No century register: explicit 1970--2069 window, not a build year. */
        year += year < 70 ? 2000 : 1900;
    }
    if (year < 1970 || day > month_days[month-1] + (month == 2 && rtc_leap(year))) return false;
    uint64_t days = 0;
    for (unsigned y = 1970; y < (unsigned)year; y++) days += rtc_leap(y) ? 366 : 365;
    for (int m = 1; m < month; m++) days += month_days[m-1] + (m == 2 && rtc_leap(year));
    days += day - 1;
    *epoch = days * 86400 + hour * 3600 + minute * 60 + second;
    return true;
}

static uint8_t rtc_read(uint8_t index) {
#ifdef SRVROS_RTC_HOST_TEST
    return test_rtc_read(index);
#else
    /* SrvrOS leaves NMI enabled. No other CMOS users run during this boot
     * sample; selecting a register preserves that policy (bit 7 clear). */
    outb(0x70,index);
    return inb(0x71);
#endif
}
static uint64_t rtc_irq_save(void) {
#ifdef SRVROS_RTC_HOST_TEST
    return 0;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}
static void rtc_irq_restore(uint64_t flags) {
#ifdef SRVROS_RTC_HOST_TEST
    (void)flags;
#else
    if (flags & (1ull << 9)) __asm__ volatile ("sti" : : : "memory");
#endif
}
static bool rtc_snapshot(struct rtc_sample *s, uint8_t century) {
    if (rtc_read(0x0a) & 0x80) return false;
    s->mode = rtc_read(0x0b); s->valid = rtc_read(0x0d);
    s->second = rtc_read(0); s->minute = rtc_read(2); s->hour = rtc_read(4);
    s->day = rtc_read(7); s->month = rtc_read(8); s->year = rtc_read(9);
    s->century = century ? rtc_read(century) : 0;
    return !(rtc_read(0x0a) & 0x80) && s->second == rtc_read(0);
}
static bool rtc_equal(const struct rtc_sample *a, const struct rtc_sample *b) {
    return a->second==b->second && a->minute==b->minute && a->hour==b->hour &&
        a->day==b->day && a->month==b->month && a->year==b->year &&
        a->century==b->century && a->mode==b->mode && a->valid==b->valid;
}
static bool rtc_fadt(const uint8_t *data, uint64_t length, uint8_t *century) {
    *century = 0;
    if (!data) return true;
    if (length >= 111 && (data[109] & 0x20)) return false;
    /* Never probe an unsupported bank, or a time/status register as century. */
    if (length >= 109 && data[108] >= 0x0e && data[108] < 0x80) *century = data[108];
    return true;
}
bool timer_wall_init(void) {
    wall_ready = false;
    uint8_t century = 0;
    for (uint64_t i = 0; i < acpi_table_count(); i++) {
        const struct acpi_table_info *t = acpi_table_at(i);
        if (t->signature[0]=='F' && t->signature[1]=='A' && t->signature[2]=='C' && t->signature[3]=='P') {
            if (!rtc_fadt(pmm_phys_to_virt(t->physical_address),t->length,&century)) {
                console_write("clock: UTC unavailable (ACPI has no CMOS RTC)\n");
                return false;
            }
            break;
        }
    }
    uint64_t start = timer_monotonic_us(), epoch = 0;
    /* Both bounds are required: an absent RTC or a stopped reference must not
     * stall boot. Only the short pair of snapshots excludes IRQ delivery. */
    for (unsigned attempt = 0; attempt < 250000; attempt++) {
        struct rtc_sample a, b;
        uint64_t flags = rtc_irq_save();
        bool stable = rtc_snapshot(&a,century) && rtc_snapshot(&b,century) && rtc_equal(&a,&b);
        uint64_t sampled_at = timer_monotonic_us();
        rtc_irq_restore(flags);
        if (stable && rtc_decode(&b,century!=0,&epoch)) {
            wall_epoch_us = epoch * 1000000; wall_base_us = sampled_at; wall_ready = true;
            break;
        }
        if (sampled_at - start >= 100000) break;
        __asm__ volatile ("pause");
    }
    console_printf("clock: UTC=%s epoch=%u century=%u\n",wall_ready?"rtc":"unavailable",epoch,(uint64_t)century);
    return wall_ready;
}
int64_t timer_realtime_us(void) {
    if (!wall_ready) return -1;
    uint64_t now = timer_monotonic_us();
    if (now < wall_base_us || now - wall_base_us > INT64_MAX - wall_epoch_us) return -1;
    return (int64_t)(wall_epoch_us + now - wall_base_us);
}
