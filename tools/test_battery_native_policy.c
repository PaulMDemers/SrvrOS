#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/battery_bounded_policy.h"
#include "../ports/srvros/uacpi/runtime/battery_native_policy.h"
#ifdef SRVROS_BATTERY_AIR61
#define TEST_GNVS_LEN 621
#define TEST_CPU_NVS_LEN 58
#else
#define TEST_GNVS_LEN 642
#define TEST_CPU_NVS_LEN 83
#endif
int main(void) {
    for(unsigned m=0;m<128;m++)assert(battery_bounded_home_entry(m&1,m&2,m&4,m&8,m&16,m&32,m&64)==(m==31));
    for(unsigned m=0;m<256;m++)assert(battery_bounded_files_entry(m&1,m&2,m&4,m&8,m&16,m&32,m&64,m&128)==(m==191));
    for(unsigned m=0;m<512;m++)assert(battery_bounded_capture_entry(m&1,m&2,m&4,m&8,m&16,m&32,m&64,m&128,m&256)==(m==63));
    for(unsigned m=0;m<256;m++)assert(battery_bounded_entry(m&1,m&2,m&4,m&8,m&16,m&32,m&64,m&128)==(m==28));
    for(unsigned a=0;a<512;a++)for(unsigned v=0;v<257;v++) {
        assert(battery_bounded_ec(false,a,v)==(a==0x60));
        assert(battery_bounded_ec(true,a,v)==(a==0x10&&!v));
    }
    assert(!battery_bounded_ec(true,0x10,UINT64_MAX));
    for(unsigned byte=0;byte<32;byte++)for(unsigned enabled=0;enabled<256;enabled++)
        for(unsigned mode=0;mode<2;mode++)
            assert(battery_native_gpe_admit(byte,enabled,mode)==
                (!enabled||(!mode&&((byte==12&&enabled==0x40)||(byte==14&&enabled==4)))));
    /* Exercise every byte of RCRB and crossing boundaries, all supported and
     * unsupported widths, and every write, independently of NVS permission. */
    const unsigned hwidths[]={0,1,8,16,24,32,64,128};
    for(int off=-8;off<0x4008;off++)for(unsigned w=0;w<8;w++){
        uint64_t a=0xfed1c000ull+off;
        assert(battery_native_hptc_allowed(a,0xfed1f404,hwidths[w],false)==(off==0x3404&&hwidths[w]==32));
        assert(!battery_native_hptc_allowed(a,0xfed1f404,hwidths[w],true));
    }
    assert(!battery_native_hptc_allowed(UINT64_MAX,0xfed1f404,32,false));
    uint64_t ig=0x8ad03190,fa=0x8ad10000;
    const unsigned widths[]={0,1,8,16,24,32,64,128};
    for(unsigned region=0;region<4;region++) {
        uint64_t base=region==0?BATTERY_GNVS:region==1?BATTERY_PNVS:region==2?ig:fa;
        unsigned len=region==0?TEST_GNVS_LEN:region==1?TEST_CPU_NVS_LEN:region==2?8192:64;
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
    /* Independently decoded OperationRegion sizes: GNVS and CpuPm PPMT / CpuSsdt PNVS. */
    assert(BATTERY_GNVS_BYTES==TEST_GNVS_LEN);
    assert(BATTERY_PNVS_BYTES==TEST_CPU_NVS_LEN);
    assert(battery_native_nvs_contains(ig,BATTERY_PNVS,TEST_CPU_NVS_LEN));
    assert(!battery_native_nvs_contains(ig,BATTERY_PNVS,TEST_CPU_NVS_LEN+1));
    assert(!battery_native_nvs_contains(ig,BATTERY_PNVS-1,TEST_CPU_NVS_LEN));
    assert(!battery_native_nvs_contains(ig,BATTERY_PNVS,0));
    for(unsigned offset=0;offset<=TEST_CPU_NVS_LEN;offset++)for(unsigned n=1;n<=TEST_CPU_NVS_LEN+1;n++)
        assert(battery_native_nvs_contains(ig,BATTERY_PNVS+offset,n)==(offset+n<=TEST_CPU_NVS_LEN));
#ifdef SRVROS_BATTERY_AIR61
    assert(!battery_native_nvs_contains(ig,0x8ad28a90,1));
    assert(!battery_native_nvs_contains(ig,0x8ad2af90,1));
#endif
    assert(!battery_native_nvs_contains(ig,UINT64_MAX,2));
    assert(!battery_native_memory_allowed(0,0,16,32,false,0));
    assert(!battery_native_memory_allowed(UINT64_MAX,UINT64_MAX,15,32,false,0));
    assert(!battery_native_memory_allowed(UINT64_MAX,fa,0x1ab,8,true,0));
    assert(!battery_native_memory_allowed(ig,fa,0xfed1f404,32,true,0x80));
    assert(!battery_native_memory_allowed(ig,fa,BATTERY_GNVS,8,true,UINT64_MAX));
    puts("battery-native-policy: PASS exact ranges, widths, values and overflow boundaries");
}
