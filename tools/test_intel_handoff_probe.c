#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/intel_handoff.h>
#include <srvros/intel_boot_claim.h>
#include <srvros/display_access.h>
#include <srvros/console.h>
#include <srvros/display_present.h>
#include <srvros/backlight.h>
#include <srvros/intel_gfx.h>
static unsigned mode,calls,flushes;static char receipt[2048];
unsigned intel_boot_policy(void){return INTEL_BOOT_OBSERVER;}
unsigned intel_boot_claimed_client(void){return mode==1?0:INTEL_BOOT_OBSERVER;}
bool display_present_handoff_idle(void){return mode!=2;}
bool console_put_pixel(uint64_t x,uint64_t y,uint32_t c){(void)x;(void)y;(void)c;calls++;assert(!display_access_allowed());return false;}
bool console_fill_rectangle(uint64_t x,uint64_t y,uint64_t w,uint64_t h,uint32_t c){(void)w;(void)h;return console_put_pixel(x,y,c);}
bool console_blit_rgb(uint64_t x,uint64_t y,uint64_t w,uint64_t h,const uint32_t *p,uint64_t stride){(void)w;(void)h;(void)stride;return console_put_pixel(x,y,*p);}
bool console_blit_rgb_scaled(uint64_t x,uint64_t y,uint64_t w,uint64_t h,const uint32_t *p,uint64_t stride,uint64_t scale){(void)scale;return console_blit_rgb(x,y,w,h,p,stride);}
int backlight_percent(unsigned v){assert(v==10 && !display_access_allowed());calls++;return -1;}
bool display_present_begin(uint64_t pid){assert(pid==1 && !display_access_allowed());calls++;return false;}
bool intel_gfx_init(void){assert(!display_access_allowed());calls++;return false;}
void console_write(const char *s){(void)s;}
bool console_sync(void){return true;}
void console_flush(void){assert(display_access_allowed());flushes++;}
void console_printf(const char *fmt,...) {
    char converted[2048];size_t n=0;
    for(size_t i=0;fmt[i];i++) {
        converted[n++]=fmt[i];
        if(fmt[i]=='%' && fmt[i+1]=='u') {
            const char *suffix=PRIu64;while(*suffix)converted[n++]=*suffix++;
            i++;
        }
    }
    converted[n]=0;va_list ap;va_start(ap,fmt);
    vsnprintf(receipt,sizeof(receipt),converted,ap);va_end(ap);
}
bool intel_window_handoff_snapshot(struct scanout_handoff_snapshot *s) {
    assert(!display_access_allowed());
    if(mode==3)return false;
    *s=(struct scanout_handoff_snapshot){.mmio=0xc0000000,.aperture=0xb0000000,.aperture_bytes=0x10000000,.fence_end=0x2200000,.first=8704,.pages=16};return true;
}
bool intel_window_handoff_revalidate(const struct scanout_handoff_snapshot *s){assert(!display_access_allowed() && !s->assurances);return mode!=4 && mode!=5;}
bool intel_window_handoff_clean(void){return mode!=5;}
int main(int argc,char **argv) {
    mode=argc>1?(unsigned)atoi(argv[1]):0;
    bool ok=intel_handoff_probe_report();assert(ok==(mode==0));
    assert(calls==(mode==1 || mode==2?0:7));
    assert(display_access_allowed()==(mode!=5));
    assert(flushes==(mode==1 || mode==5?0:1));
    assert(strstr(receipt,"hardware_ownership=0") && strstr(receipt,"managed_pages=0"));
    assert(!intel_handoff_probe_report());puts(receipt);return 0;
}
