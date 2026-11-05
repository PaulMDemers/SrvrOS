#include <srvros/haswell_firmware.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t table[136],pin[32];
static unsigned phase,identities,catalogs,tables,mode;
static uint64_t clock_us;
static bool ident(void *c,uint32_t out[5]){(void)c;uint32_t v[5]={0x0a268086,0x03000009,0x011a106b,0x0a048086,0x9c438086};memcpy(out,v,sizeof(v));identities++;if(mode==1)out[0]=0;return true;}
static bool catalog(void *c,struct hfw_entry *out,unsigned *n){(void)c;catalogs++;*n=2;out[0]=(struct hfw_entry){0x100000,0x1000,2};out[1]=(struct hfw_entry){0x200000,0x1000,0};if(mode==2)out[1].base=0x100000;if(mode==3&&phase)out[1].length+=4096;return true;}
static bool readtable(void *c,const struct hfw_entry *map,unsigned n,uint8_t *out,uint64_t *pa){(void)c;(void)map;(void)n;tables++;memcpy(out,table,136);*pa=0x100100;if(mode==4)out[60]^=1;if(mode==5)*pa=0x200000;return true;}
static uint64_t now(void *c){(void)c;clock_us+=mode==6?100001:100;return clock_us;}
static void progress(void *c,unsigned p){(void)c;phase=p;}
int main(void){
 memcpy(table,"DMAR",4);table[4]=136;table[8]=1;unsigned sum=0;for(unsigned i=0;i<136;i++)sum+=table[i];table[9]=(uint8_t)-sum;hfw_sha256(table,pin);
 struct hfw_io io={.identity=ident,.catalog=catalog,.table=readtable,.now=now,.progress=progress};struct hfw_result r;
 assert(hfw_capture(&r,&io,pin) && r.status==1 && r.sample[0].count==2);
 for(mode=1;mode<=6;mode++){identities=catalogs=tables=0;clock_us=0;assert(!hfw_capture(&r,&io,pin));if(mode==1)assert(catalogs==0&&tables==0);if(mode==2)assert(tables==0);}
 struct hfw_entry entries[128];for(unsigned i=0;i<128;i++)entries[i]=(struct hfw_entry){0x100000+4096*i,4096,2};
 assert(hfw_catalog_valid(entries,128));assert(!hfw_catalog_valid(entries,129));assert(!hfw_catalog_valid(entries,0));
 entries[1].type=9;assert(!hfw_catalog_valid(entries,128));entries[1].type=2;
 entries[1].length=0;assert(!hfw_catalog_valid(entries,128));entries[1].length=4096;
 entries[1].base=UINT64_MAX-100;assert(!hfw_catalog_valid(entries,128));
 for(unsigned kind=0;kind<=8;kind++){entries[0].type=kind;assert(hfw_table_covered(entries,1,0x100100)==(kind==2||kind==3));}
 entries[0].type=2;
 assert(hfw_table_covered(entries,1,0x100100));assert(!hfw_table_covered(entries,1,0x100fff));
 /* Hash vectors independently checked by the Python host test. */
 for(unsigned i=0;i<136;i++)table[i]=(uint8_t)i;
 hfw_sha256(table,pin);for(unsigned i=0;i<32;i++)printf("%02x",pin[i]);puts("");
 memset(table,0,136);hfw_sha256(table,pin);for(unsigned i=0;i<32;i++)printf("%02x",pin[i]);puts("");
 puts("haswell-firmware core PASS");return 0;
}
