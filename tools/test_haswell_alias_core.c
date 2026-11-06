#include <srvros/haswell_alias_context.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct hac_result r;
static uint64_t ticks,words[65][512];static unsigned reads,mode;
static uint64_t now(void *x){(void)x;return mode==7?1:mode==8?(ticks+=100001):++ticks;}
static bool read_word(void *x,uint64_t pa,unsigned i,uint64_t *v){
 (void)x;assert(pa!=0xfed90000 && pa>=0x100000 && pa<0x141000 && i<512);reads++;
 if(mode==6)return false;
 *v=words[(pa-0x100000)/4096][i];if(mode==5 && reads>2048)*v^=32;return true;
}
static void setup(void){memset(words,0,sizeof(words));ticks=reads=mode=0;words[0][0]=0x101003;words[1][0]=0x102003;words[2][0]=0x103003;words[3][0]=0xfed90019;}
int main(void){
 const struct hac_io io={.read=read_word,.now=now};
 setup();assert(hac_capture(&r,&io,0x100000) && r.pages==4 && r.reads[0]==2048 && r.reads[1]==2048 && r.target_leaves==1 && r.bytes[0]==r.bytes[1] && !memcmp(r.data[0],r.data[1],r.bytes[0]));
 setup();words[1][0]=0xc0000083;assert(hac_capture(&r,&io,0x100000) && r.pages==2 && r.target_leaves==1);
 setup();words[2][0]=0xfec00083;assert(hac_capture(&r,&io,0x100000) && r.pages==3 && r.target_leaves==1);
 setup();words[0][1]=words[0][0];assert(hac_capture(&r,&io,0x100000) && r.pages==4);
 for(unsigned m=5;m<=8;m++){setup();mode=m;assert(!hac_capture(&r,&io,0x100000));if(m==5)assert(r.cursor_expected!=r.cursor_observed);}
 setup();words[0][0]=0x100003;assert(!hac_capture(&r,&io,0x100000) && r.status==4);
 setup();words[1][0]=0xc0001083;assert(hac_capture(&r,&io,0x100000)); /* Huge PAT is not a PA bit. */
 setup();words[1][0]=0xc0002083;assert(!hac_capture(&r,&io,0x100000));
 setup();words[0][0]|=1ull<<39;assert(!hac_capture(&r,&io,0x100000));
 setup();memset(words,0,sizeof(words));for(unsigned i=0;i<63;i++)words[0][i]=(0x101000+4096*i)|3;assert(hac_capture(&r,&io,0x100000) && r.pages==64 && r.reads[0]==32768 && r.reads[1]==32768);
 setup();for(unsigned i=0;i<64;i++)words[0][i]=(0x101000+4096*i)|3;assert(!hac_capture(&r,&io,0x100000) && r.status==2);
 setup();words[0][0]=0;for(unsigned i=0;i<512;i++)words[0][i]=(uint64_t)(i*i*17)<<12;assert(!hac_capture(&r,&io,0x100000) && r.status==2); /* Lossless output exhaustion. */
 setup();assert(!hac_capture(&r,&io,0x100001));
 puts("haswell-alias-core PASS complete/shared/4KiB/2MiB/1GiB graphs, raw equality, source/change/clock/cycle/physical/table/output bounds; no target loads");return 0;
}
