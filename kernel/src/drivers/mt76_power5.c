#include <srvros/mt76_power5.h>
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static uint16_t word(const uint8_t *p,unsigned a){return p[a]|(uint16_t)p[a+1]<<8;}
static bool profile(const struct mt76_power5_fields *f){return f && f->generation && f->generation!=UINT64_MAX && f->chip==0x7610 && f->nic0==0xfd11 && f->nic1==0x1004 && (f->channel_pair>>8)<=63;}
enum mt76_power5_status mt76_power5_fields(const uint8_t *p,size_t n,uint64_t generation,struct mt76_power5_fields *out){
 if(!out || (p && overlap(out,sizeof *out,p,n)))return MT76_POWER5_INVALID;
 scrub(out,sizeof *out);
 if(!p || n!=512 || !generation || generation==UINT64_MAX)return MT76_POWER5_INVALID;
 struct mt76_power5_fields f={.generation=generation,.chip=word(p,0),.nic0=word(p,0x34),.nic1=word(p,0x36),.channel_pair=word(p,0x98),.cck=word(p,0xde),.ofdm_lo=word(p,0x120),.ofdm_hi=word(p,0x122),.ht_lo=word(p,0x124),.ht_hi=word(p,0x126),.vht=word(p,0x12c)};
 bool ok=profile(&f);if(ok)*out=f;scrub(&f,sizeof f);return ok?MT76_POWER5_OK:MT76_POWER5_UNSUPPORTED;
}
static int s6(unsigned x){x&=63;return x&32?(int)x-64:(int)x;}
static uint32_t pack(int a,int b,int c,int d){const int x[]={a,b,c,d};uint32_t v=0;for(unsigned i=0;i<4;i++){unsigned n=(unsigned)(x[i]<0?x[i]+64:x[i]);v|=(uint32_t)n<<(8*i);}return v;}
enum mt76_power5_status mt76_power5_plan(const struct mt76_power5_fields *f,
 const uint8_t country[2],unsigned channel,unsigned bandwidth,unsigned limit,struct mt76_power5_plan *out){
 if(!out || (f && overlap(out,sizeof *out,f,sizeof *f)) || (country && overlap(out,sizeof *out,country,2)))return MT76_POWER5_INVALID;
 scrub(out,sizeof *out);
 if(!f || !country || !f->generation || f->generation==UINT64_MAX)return MT76_POWER5_INVALID;
 if(!profile(f) || country[0]!='U' || country[1]!='S' || channel!=161 || bandwidth!=20 || limit>30)return MT76_POWER5_UNSUPPORTED;
 /* Same ordering as mt76x02_rate_power:CCK4,OFDM8,HT16,VHT2. No BW
  * delta for20MHz and no TSSI compensation for the captured board profile. */
 int rates[30]={0};const uint16_t words[]={f->cck,f->ofdm_lo,f->ofdm_hi,f->ht_lo,f->ht_hi};
 for(unsigned i=0;i<5;i++){rates[4*i]=rates[4*i+1]=s6(words[i]);rates[4*i+2]=rates[4*i+3]=s6(words[i]>>8);}
 rates[28]=s6(f->vht);rates[29]=s6(f->vht>>8);
 unsigned base=f->channel_pair>>8,maximum=0;
 for(unsigned i=0;i<30;i++){
  int absolute=(int)base+rates[i];if(absolute>(int)limit)absolute=(int)limit;
  int relative=absolute-(int)base;
  if(relative < -32 || relative>31){scrub(rates,sizeof rates);return MT76_POWER5_UNSUPPORTED;}
  rates[i]=relative;if(absolute>(int)maximum)maximum=(unsigned)absolute;
 }
 struct mt76_power5_plan p={.valid=true,.generation=f->generation,.factory_base=base,.limit_half_dbm=limit,.maximum_half_dbm=maximum};
 p.write[0]=(struct mt76_power5_write){0x13b0,0x3f3f,base|(base<<8)};
 const uint32_t addresses[]={0x1314,0x1318,0x131c,0x1320,0x1324,0x13d4,0x13d8,0x13dc};
 for(unsigned i=0;i<8;i++)p.write[i+1]=(struct mt76_power5_write){.address=addresses[i],.mask=UINT32_MAX};
 p.write[1].value=pack(rates[0],rates[2],rates[4],rates[6]);
 p.write[2].value=pack(rates[8],rates[10],rates[12],rates[14]);
 p.write[3].value=pack(rates[16],rates[18],rates[20],rates[22]);
 p.write[4].value=pack(rates[24],rates[26],rates[12],rates[14]);
 p.write[5].value=pack(rates[16],rates[18],0,0);
 /* MT7610 CFG7 contains OFDM54 and HT7 only;the shared x02
  * writer also fills x2 VHT bytes which this chip reads back as zero.
  * MT76x0_WriteNewPerRatePwr/AsicExtraPowerOverMAC place VHT8/9
  * solely in CFG8. Program reserved positions as zero,not masked success. */
 p.write[6].value=pack(rates[11],0,rates[19],0);
 p.write[7].value=pack(rates[26],0,rates[28],rates[29]);
 p.write[8].value=pack(rates[19],0,0,0);
 *out=p;scrub(rates,sizeof rates);scrub(&p,sizeof p);return MT76_POWER5_OK;
}
