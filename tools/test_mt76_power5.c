#include <srvros/mt76_power5.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct mt76_power5_fields fields(void){return (struct mt76_power5_fields){.generation=1,.chip=0x7610,.nic0=0xfd11,.nic1=0x1004,.channel_pair=19u<<8,.cck=0x0201,.ofdm_lo=0x0405,.ofdm_hi=0x0004,.ht_lo=0x0607,.ht_hi=0x0203,.vht=0x0809};}
static enum mt76_power5_status plan(struct mt76_power5_fields *f,unsigned limit,struct mt76_power5_plan *p){const uint8_t us[]={'U','S'};return mt76_power5_plan(f,us,161,20,limit,p);}
static void empty(const void *p,size_t n){for(size_t i=0;i<n;i++)assert(!((const uint8_t *)p)[i]);}
static int decode(unsigned x){x&=63;return x<32?(int)x:(int)x-64;}
static void put16(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
int main(void){
 struct mt76_power5_fields f=fields(),saved=f;struct mt76_power5_plan p;
 assert(plan(&f,30,&p)==MT76_POWER5_OK && p.valid && p.generation==1 && p.factory_base==19 && p.maximum_half_dbm==28);
 const uint32_t address[]={0x13b0,0x1314,0x1318,0x131c,0x1320,0x1324,0x13d4,0x13d8,0x13dc};
 const uint32_t values[]={0x1313,0x04050201,0x06070004,0x00000203,0x06070000,0x00000203,0x00020000,0x08090000,0x00000002};
 for(unsigned i=0;i<9;i++)assert(p.write[i].address==address[i] && p.write[i].value==values[i] && p.write[i].mask==(i?UINT32_MAX:0x3f3f));
 assert(((0x2f2f1b1b&~p.write[0].mask)|p.write[0].value)==0x2f2f1313 && !memcmp(&f,&saved,sizeof f));
 unsigned cases=0;
 for(unsigned base=0;base<=63;base++)for(unsigned raw=0;raw<64;raw++)for(unsigned limit=0;limit<=30;limit++){
  f=fields();f.channel_pair=(uint16_t)(base<<8);f.ofdm_lo=(uint16_t)(raw|(raw<<8));
  enum mt76_power5_status status=plan(&f,limit,&p);
  if(base>limit+32){assert(status==MT76_POWER5_UNSUPPORTED);empty(&p,sizeof p);continue;}
  assert(status==MT76_POWER5_OK && p.maximum_half_dbm<=limit);
  int effective=(int)base+decode(p.write[1].value>>16);int wanted=(int)base+decode(raw);if(wanted>(int)limit)wanted=(int)limit;
  assert(effective==wanted);
  for(unsigned i=1;i<9;i++){assert(!(p.write[i].value&0xc0c0c0c0u));for(unsigned b=0;b<4;b++){
   /* Some unused fields are explicit0,not a selected TX rate. */
   int value=decode(p.write[i].value>>(8*b));assert(value>=-32 && value<=31);
  }}cases++;
 }
 /* Captured v123 failure: shared x02 duplicated VHT -4 in CFG7,
  * but MT7610 returned only the documented OFDM54/HT7 fields. */
 f=fields();f.cck=0;f.ht_lo=0x0202;f.ht_hi=0x3e02;f.vht=0x3c3c;
 assert(plan(&f,30,&p)==MT76_POWER5_OK);
 assert(p.write[6].value==0x003e0000 && p.write[7].value==0x3c3c0000 && p.write[8].value==0x3e);
 assert((0x3c3e3c00u&0x003f003fu)==p.write[6].value && p.write[6].value!=0x3c3e3c00);
 for(unsigned i=6;i<9;i++)assert(p.write[i].mask==UINT32_MAX); /* exact readbacks stay */
 f=fields();
 for(unsigned field=0;field<3;field++)for(unsigned bit=0;bit<16;bit++){
  f=fields();if(field==0)f.chip^=1u<<bit;if(field==1)f.nic0^=1u<<bit;if(field==2)f.nic1^=1u<<bit;
  assert(plan(&f,30,&p)==MT76_POWER5_UNSUPPORTED);empty(&p,sizeof p);
 }
 f=fields();f.channel_pair=0xff00;assert(plan(&f,30,&p)==MT76_POWER5_UNSUPPORTED);empty(&p,sizeof p);
 f=fields();assert(plan(&f,31,&p)==MT76_POWER5_UNSUPPORTED);empty(&p,sizeof p);
 const uint8_t us[]={'U','S'},bad[]={'C','A'};
 assert(mt76_power5_plan(&f,bad,161,20,30,&p)==MT76_POWER5_UNSUPPORTED);empty(&p,sizeof p);
 assert(mt76_power5_plan(&f,us,1,20,30,&p)==MT76_POWER5_UNSUPPORTED);empty(&p,sizeof p);
 assert(mt76_power5_plan(&f,us,161,80,30,&p)==MT76_POWER5_UNSUPPORTED);empty(&p,sizeof p);
 f=fields();saved=f;assert(plan(&f,30,(void *)&f)==MT76_POWER5_INVALID && !memcmp(&f,&saved,sizeof f));
 uint8_t blob[513]={0};
 const unsigned offsets[]={0,0x34,0x36,0x98,0xde,0x120,0x122,0x124,0x126,0x12c};
 const unsigned ee[]={0x7610,0xfd11,0x1004,19u<<8,0x0201,0x0405,4,0x0607,0x0203,0x0809};
 for(unsigned i=0;i<10;i++)put16(blob+offsets[i],ee[i]);
 memset(blob+4,0xa5,6);
 assert(mt76_power5_fields(blob,512,1,&f)==MT76_POWER5_OK && !memcmp(&f,&saved,sizeof f));
 for(unsigned n=0;n<512;n++){memset(&f,0xa5,sizeof f);assert(mt76_power5_fields(blob,n,1,&f)==MT76_POWER5_INVALID);empty(&f,sizeof f);}
 assert(mt76_power5_fields(blob,513,1,&f)==MT76_POWER5_INVALID);empty(&f,sizeof f);
 assert(mt76_power5_fields(blob,512,0,&f)==MT76_POWER5_INVALID);empty(&f,sizeof f);
 uint8_t copy[513];memcpy(copy,blob,sizeof blob);assert(mt76_power5_fields(blob,512,1,(void *)blob)==MT76_POWER5_INVALID && !memcmp(blob,copy,sizeof blob));
 printf("mt76-power5: PASS golden nine-register masks/words,%u signed6/base/ceiling combinations,unknown/TSSI/band/width/invalid field refusal,all logical blob truncations/aliases;model only,no fresh EFUSE/TX/analog power authority\n",cases);return 0;
}
