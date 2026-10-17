#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static uint32_t counter, step;
static uint32_t test_pm_read(void) { uint32_t value = counter; counter += step; return value; }
#define SRVROS_CLOCK_HOST_TEST
#include "../kernel/src/arch/x86_64/clock.c"
static uint8_t fadt[244];
static struct acpi_table_info info;
uint64_t acpi_table_count(void) { return 1; }
const struct acpi_table_info *acpi_table_at(uint64_t i) { assert(i == 0); return &info; }
void *pmm_phys_to_virt(uint64_t address) { (void)address; return fadt; }
void console_printf(const char *fmt, ...) { (void)fmt; }
uint64_t timer_ticks(void) { return 42; }
int main(void) {
    fadt[76]=8; fadt[77]=4; fadt[91]=4;
    assert(fadt_timer_port(fadt,115)==0);
    assert(fadt_timer_port(fadt,116)==0x408);
    fadt[208]=1; fadt[212]=0x18; fadt[213]=4;
    assert(fadt_timer_port(fadt,sizeof(fadt))==0x418);
    fadt[208]=0; assert(fadt_timer_port(fadt,sizeof(fadt))==0); fadt[208]=1;
    fadt[114]=0x10; assert(fadt_timer_port(fadt,sizeof(fadt))==0); fadt[114]=0;
    memcpy(info.signature,"FACP",4); info.length=sizeof(fadt);
    step=1; assert(timer_reference_init());
    step=0; counter=PM_FREQUENCY; assert(timer_monotonic_us()==1000000);
    last_counter=PM_MASK-3; counter=6; accumulated=0;
    assert(timer_monotonic_us()==10*1000000/PM_FREQUENCY);
    assert(timer_monotonic_us()==10*1000000/PM_FREQUENCY);
    step=100; assert(timer_delay_us(50));
    step=0; assert(!timer_reference_init());
    assert(!timer_reference_ready() && !timer_delay_us(50));
    assert(timer_monotonic_us()==420000);
    fadt[91]=0; step=1; assert(!timer_reference_init());
    puts("clock-host: PASS (FADT bounds/address spaces, stopped timer, wrap, deadlines)");
}
