#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/battery_native_policy.h"
int main(void) {
    for(unsigned byte=0;byte<32;byte++)for(unsigned enabled=0;enabled<256;enabled++)
        for(unsigned mode=0;mode<2;mode++)
            assert(battery_native_gpe_admit(byte,enabled,mode)==
                (!enabled||(!mode&&((byte==12&&enabled==0x40)||(byte==14&&enabled==4)))));
    uint64_t ig=0x8ad03190,fa=0x8ad10000;
    const unsigned widths[]={0,1,8,16,24,32,64,128};
    for(unsigned region=0;region<4;region++) {
        uint64_t base=region==0?BATTERY_GNVS:region==1?BATTERY_PNVS:region==2?ig:fa;
        unsigned len=region==0?0x282:region==1?83:region==2?8192:64;
        for(int offset=-8;offset<(int)len+8;offset++)for(unsigned w=0;w<8;w++)for(unsigned value=0;value<=256;value++) {
            uint64_t address=base+offset;unsigned bits=widths[w];
            bool write=bits==8&&((region==0&&(offset==0||offset==1)&&value<=255)||
                (region==0&&(offset==15||offset==16)&&value<=1)||
                (region==2&&offset==0x1ac&&value<=1)||
                (region==2&&offset>0x1ac&&offset<=0x1af&&!value));
            assert(battery_native_memory_allowed(ig,fa,address,bits,true,value)==write);
            bool read=(bits==8||bits==16||bits==32||bits==64)&&offset>=0&&offset+(int)(bits/8)<=(int)len;
            if(region==3)read=offset==16&&bits==32;
            assert(battery_native_memory_allowed(ig,fa,address,bits,false,value)==read);
        }
    }
    /* Independent firmware contract: CpuSsdt declares 0x0053, not decimal53.
     * Exercise the same predicate used by the real native map callback. */
    assert(BATTERY_PNVS_BYTES==83);
    assert(battery_native_nvs_contains(ig,BATTERY_PNVS,83));
    assert(!battery_native_nvs_contains(ig,BATTERY_PNVS,84));
    assert(!battery_native_nvs_contains(ig,BATTERY_PNVS-1,83));
    assert(!battery_native_nvs_contains(ig,BATTERY_PNVS,0));
    for(unsigned offset=0;offset<84;offset++)for(unsigned n=1;n<=84;n++)
        assert(battery_native_nvs_contains(ig,BATTERY_PNVS+offset,n)==(offset+n<=83));
    assert(!battery_native_nvs_contains(ig,UINT64_MAX,2));
    assert(!battery_native_memory_allowed(0,0,16,32,false,0));
    assert(!battery_native_memory_allowed(UINT64_MAX,UINT64_MAX,15,32,false,0));
    assert(!battery_native_memory_allowed(UINT64_MAX,fa,0x1ab,8,true,0));
    assert(!battery_native_memory_allowed(ig,fa,0xfed1f404,32,true,0x80));
    assert(!battery_native_memory_allowed(ig,fa,BATTERY_GNVS,8,true,UINT64_MAX));
    puts("battery-native-policy: PASS exact ranges, widths, values and overflow boundaries");
}
