#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {struct mt76_pa5 *s;unsigned calls,auth,fail,deny,bad,corrupt,byte,bit;int size;uint32_t regs[14];};
static struct mt76_pa5 states[490],good;static bool save;
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool transfer(void *p,bool wr,uint32_t a,uint32_t *v){
 struct fake *f=p;unsigned op=f->calls++,n=op%7,q=op/7,i=mt76_pa5_index(q);
 if(save)states[op]=*f->s;
 assert(mt76_pa5_transfer_valid(f->s,op,wr,a,*v));
 assert(!mt76_pa5_transfer_valid(f->s,op,!wr,a,*v));
 assert(!mt76_pa5_transfer_valid(f->s,op,wr,a^4,*v));
 if(wr)assert(!mt76_pa5_transfer_valid(f->s,op,wr,a,*v^1));
 if(f->fail==op+1)return false;
 if(n==3){if(wr)f->regs[i]=*v;else *v=f->regs[i];}else{unsigned j=n<3?n:n-4;*v=j==0?0:j==1?0xf000f200:0x40000050;}
 if(f->bad==op+1)*v^=1;
 return true;
}
static int exchange(void *p,const uint8_t *packet,size_t len,uint8_t *reply,size_t cap){
 struct fake *f=p;unsigned op=f->calls++,q=op/7,i=mt76_pa5_index(q);uint8_t expected[16];
 if(save)states[op]=*f->s;
 assert(len==16 && cap==1024 && mt76_pa5_packet(f->s,op,expected,16)==16 && !memcmp(expected,packet,16));
 uint32_t w[4]={0};for(unsigned b=0;b<16;b++)w[b/4]|=(uint32_t)packet[b]<<(8*(b%4));
 assert(w[0]==(0x50000008u|((mt76_pa5_write(q)?12u:10u)<<20)|((q%15+1)<<16)) && w[1]==mt76_pa5_target(i) && !w[3]);
 assert(w[2]==(mt76_pa5_write(q)?mt76_pa5_expected(q):0));
 if(f->fail==op+1)return -1;
 if(mt76_pa5_write(q))f->regs[i]=w[2];
 w[0]=w[3]=0x4a000008u|((q%15+1)<<16);w[2]=f->regs[i];memset(reply,0,cap);
 for(unsigned b=0;b<16;b++)reply[b]=(uint8_t)(w[b/4]>>(8*(b%4)));
 if(f->corrupt==op+1)reply[f->byte]^=1u<<f->bit;
 return f->size;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_pa5 *s){f->s=s;for(unsigned i=0;i<14;i++)f->regs[i]=mt76_pa5_initial(i);const struct mt76_pa5_io io={f,auth,transfer,exchange};return mt76_pa5_capture(s,0xfd11,0x1004,&io);}
int main(int argc,char **argv){
 assert(argc==2);FILE *b=fopen(argv[1],"rb");assert(b);for(unsigned i=0;i<14;i++)for(unsigned j=0;j<3;j++){uint32_t v=0;for(unsigned n=0;n<4;n++){int c=fgetc(b);assert(c>=0);v|=(uint32_t)c<<(8*n);}assert(v==(j==0?mt76_pa5_target(i):j==1?mt76_pa5_initial(i):mt76_pa5_selected(i)));}assert(fgetc(b)==EOF);fclose(b);
 struct mt76_pa5 s={0};struct fake f={.size=16};save=true;assert(run(&f,&s)==MT76_TIMING_OK && mt76_pa5_valid(&s));save=false;good=s;
 assert(f.calls==490 && f.auth==491 && s.writes==28 && s.exchanges==55 && s.reads==429);for(unsigned i=0;i<14;i++)assert(f.regs[i]==mt76_pa5_initial(i));assert(run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned n=1;n<=490;n++){
  f=(struct fake){.size=16,.fail=n};s=(struct mt76_pa5){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.calls==n);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  s=states[n-1];assert(mt76_pa5_admit(&s,n-1));s.reads++;assert(!mt76_pa5_admit(&s,n-1));
  if(!mt76_pa5_rf(n-1)){f=(struct fake){.size=16,.bad=n};s=(struct mt76_pa5){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete);}
 }
 for(unsigned n=1;n<=491;n++){f=(struct fake){.size=16,.deny=n};s=(struct mt76_pa5){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(unsigned q=0;q<70;q++)if(mt76_pa5_rf(q*7+3))for(unsigned byte=0;byte<16;byte++)for(unsigned bit=0;bit<8;bit++){f=(struct fake){.size=16,.corrupt=q*7+4,.byte=byte,.bit=bit};s=(struct mt76_pa5){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete);}
 for(unsigned q=0;q<70;q++)for(unsigned j=0;j<4;j++)for(unsigned bit=0;bit<32;bit++){s=good;s.words[q][j]^=1u<<bit;assert(!mt76_pa5_valid(&s));}
 for(unsigned q=0;q<140;q++)for(unsigned j=0;j<3;j++){s=good;s.guards[q][j]^=1;assert(!mt76_pa5_valid(&s));}
 const int sizes[]={0,1,4,8,12,15,17,1024,1025};for(unsigned i=0;i<sizeof sizes/sizeof sizes[0];i++){f=(struct fake){.size=sizes[i]};s=(struct mt76_pa5){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete);}
 for(unsigned bit=0;bit<16;bit++){const struct mt76_pa5_io io={&f,auth,transfer,exchange};s=(struct mt76_pa5){0};assert(mt76_pa5_capture(&s,0xfd11^(1u<<bit),0x1004,&io)==MT76_TIMING_REFUSED && !s.attempted);assert(mt76_pa5_capture(&s,0xfd11,0x1004^(1u<<bit),&io)==MT76_TIMING_REFUSED && !s.attempted);}
 struct mt76_5ghz_prereq parent={.attempted=true,.complete=true,.operations=308,.reads=294,.exchanges=14,.observed=44,.received=16};
 for(unsigned i=0;i<88;i++){parent.guards[i][1]=0xf000f200;parent.guards[i][2]=0x40000050;}
 for(unsigned i=0;i<14;i++)parent.values[i==0?0:i<12?29+i:i==12?1:2]=mt76_pa5_initial(i);
 for(unsigned i=30;i<44;i++){uint32_t h=0x4a000008u|((i-29)<<16);parent.words[i-30][0]=parent.words[i-30][3]=h;parent.words[i-30][1]=mt76_frontend_target(i);parent.words[i-30][2]=parent.values[i];}
 assert(mt76_pa5_parent_valid(&parent));
 for(unsigned i=0;i<14;i++){unsigned j=i==0?0:i<12?29+i:i==12?1:2;parent.values[j]^=1;if(j>=30)parent.words[j-30][2]^=1;assert(mt76_frontend_valid(&parent) && !mt76_pa5_parent_valid(&parent));parent.values[j]^=1;if(j>=30)parent.words[j-30][2]^=1;}
 parent.complete=false;assert(!mt76_pa5_parent_valid(&parent));
 assert(mt76_pa5_target(14)==UINT32_MAX && mt76_pa5_address(490)==UINT32_MAX && !mt76_pa5_valid(NULL));
 puts("PA5 cycle: PASS14 pinned originals/select/readback/reverse/readback,490 I/O/491 authorization boundaries,all reply/proof bits,board faults,no arbitrary write or replay");
}
