#include <srvros/haswell_alias_context.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct hac_result r;
static uint64_t ticks,words[65][512];static unsigned reads,mode;
static uint64_t now(void *x){(void)x;if((mode==14 && reads>=2048) || (mode==15 && reads>=4096))return ticks+=100001;return mode==7?1:mode==8?(ticks+=100001):++ticks;}
static bool read_word(void *x,uint64_t pa,unsigned i,uint64_t *v){
 (void)x;assert(pa!=0xfed90000 && pa>=0x100000 && pa<0x141000 && i<512);reads++;
 if(mode==6 || (mode==13 && reads>2048))return false;
 /* Model an HHDM A-bit set by first reading a later table page. */
 if(mode==9 && pa==0x103000)words[0][0]|=32;
 if(mode==10 && reads>4096)words[0][0]|=32;
 if(mode==11 && reads>4096)words[0][0]|=64;
 if(mode==12 && reads>4096)words[0][0]^=4096;
 if(mode==16 && reads>2048)words[0][1]=0x104003;
 if(mode==18 && reads==1){for(unsigned a=0;a<HAC_TABLE_CAP;a++)for(unsigned b=0;b<512;b++)assert(!r.table[a].word[b]);for(unsigned a=0;a<2;a++)for(unsigned b=0;b<HAC_DATA_CAP;b++)assert(!r.data[a][b]);}
 *v=words[(pa-0x100000)/4096][i];if(mode==5 && reads>4096)*v^=32;return true;
}
static void setup(void){memset(words,0,sizeof(words));ticks=reads=mode=0;words[0][0]=0x101003;words[1][0]=0x102003;words[2][0]=0x103003;words[3][0]=0xfed90019;}
static void setup_large_graph(void) {
 setup();words[3][0]=0;
 for(unsigned page=3;page<42;page++) {
  words[2][page-3]=(0x100000ull+4096*page)|3;
  uint64_t base=0x8000000020000000ull+(uint64_t)page*0x200000;
  for(unsigned i=13;i<76;i++)words[page][i]=base+4096*i+0x23;
  for(unsigned i=91;i<118;i++)words[page][i]=base+4096*i+0x63;
  for(unsigned i=126;i<502;i++)words[page][i]=base+4096*i+0x23;
  words[page][509]=0xfffffffffffffff0ull;
  words[page][511]=0x10;
 }
}
int main(int argc,char **argv){
 const struct hac_io io={.read=read_word,.now=now};
 setup_large_graph();
 assert(hac_capture(&r,&io,0x100000) && r.pages==42 && r.prepare_pages==42);
 assert(r.prepare_reads==21504 && r.reads[0]==21504 && r.reads[1]==21504);
 assert(r.bytes[0]<=HAC_DATA_CAP && r.bytes[0]==r.bytes[1]);
 assert(!memcmp(r.data[0],r.data[1],r.bytes[0]));
 if(argc==2 && !strcmp(argv[1],"--dump-large")) {
  for(unsigned pass=0;pass<2;pass++)
   if(fwrite(r.data[pass],1,r.bytes[pass],stdout)!=r.bytes[pass])return 1;
  return 0;
 }
 setup();assert(hac_capture(&r,&io,0x100000) && r.pages==4 && r.prepare_pages==4 && r.prepare_reads==2048 && r.begin<r.prepare_end && r.prepare_end<r.end && r.reads[0]==2048 && r.reads[1]==2048 && r.target_leaves==1 && r.bytes[0]==r.bytes[1] && !memcmp(r.data[0],r.data[1],r.bytes[0]));
 setup();words[1][0]=0xc0000083;assert(hac_capture(&r,&io,0x100000) && r.pages==2 && r.target_leaves==1);
 setup();words[2][0]=0xfec00083;assert(hac_capture(&r,&io,0x100000) && r.pages==3 && r.target_leaves==1);
 setup();words[0][1]=words[0][0];assert(hac_capture(&r,&io,0x100000) && r.pages==4);
 for(unsigned m=5;m<=8;m++){setup();mode=m;assert(!hac_capture(&r,&io,0x100000));if(m==5)assert(r.cursor_expected!=r.cursor_observed);}
 setup();mode=9;assert(hac_capture(&r,&io,0x100000) && r.table[0].word[0]==0x101023 && r.prepare_reads==2048 && reads==6144);
 for(unsigned m=10;m<=13;m++){setup();mode=m;assert(!hac_capture(&r,&io,0x100000));assert(r.status==(m==13?3:5));}
 for(unsigned m=14;m<=16;m++){setup();mode=m;assert(!hac_capture(&r,&io,0x100000));assert(r.status==(m==16?5:2));}
 setup();memset(&r,0xa5,sizeof(r));mode=18;assert(hac_capture(&r,&io,0x100000));
 setup();words[0][0]=0x100003;assert(!hac_capture(&r,&io,0x100000) && r.status==4);
 setup();words[1][0]=0xc0001083;assert(hac_capture(&r,&io,0x100000)); /* Huge PAT is not a PA bit. */
 setup();words[1][0]=0xc0002083;assert(!hac_capture(&r,&io,0x100000));
 setup();words[0][0]|=1ull<<39;assert(!hac_capture(&r,&io,0x100000));
 setup();memset(words,0,sizeof(words));for(unsigned i=0;i<63;i++)words[0][i]=(0x101000+4096*i)|3;assert(hac_capture(&r,&io,0x100000) && r.pages==64 && r.reads[0]==32768 && r.reads[1]==32768);
 setup();for(unsigned i=0;i<64;i++)words[0][i]=(0x101000+4096*i)|3;assert(!hac_capture(&r,&io,0x100000) && r.status==2);
 setup();
 for(unsigned i=0;i<512;i++)words[0][i]=((uint64_t)(i*i*17)<<12)|((i&1)?0xb570000000000000ull:0xa550000000000000ull);
 assert(!hac_capture(&r,&io,0x100000) && r.status==2 && r.reads[1]==0); /* Lossless output exhaustion. */
 setup();assert(!hac_capture(&r,&io,0x100001));
 puts("haswell-alias-core PASS complete/shared/4KiB/2MiB/1GiB graphs, 42-table compact receipt, bounded first-touch preparation, exact A/D/address-change rejection, source/change/clock/cycle/physical/table/output bounds; no target loads");return 0;
}
