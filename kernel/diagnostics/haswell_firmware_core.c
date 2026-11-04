#include <srvros/haswell_firmware.h>
#include <stddef.h>
static uint32_t rotr(uint32_t v,unsigned n){return (v>>n)|(v<<(32-n));}
/* Fixed136-byte input: exactly three SHA256 blocks, no dynamic allocation. */
void hfw_sha256(const uint8_t input[HFW_DMAR_BYTES],uint8_t out[32]) {
 static const uint32_t k[64]={
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 uint8_t bytes[192]={0};for(unsigned i=0;i<HFW_DMAR_BYTES;i++)bytes[i]=input[i];
 bytes[HFW_DMAR_BYTES]=0x80;bytes[190]=4;bytes[191]=0x40; /*136*8=1088*/
 uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 for(unsigned block=0;block<3;block++) {
  uint32_t w[64];for(unsigned i=0;i<16;i++){unsigned p=64*block+4*i;w[i]=(uint32_t)bytes[p]<<24|(uint32_t)bytes[p+1]<<16|(uint32_t)bytes[p+2]<<8|bytes[p+3];}
  for(unsigned i=16;i<64;i++){uint32_t a=w[i-15],b=w[i-2];w[i]=w[i-16]+(rotr(a,7)^rotr(a,18)^(a>>3))+w[i-7]+(rotr(b,17)^rotr(b,19)^(b>>10));}
  uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
  for(unsigned i=0;i<64;i++){uint32_t t=z+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];uint32_t u=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));z=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;}
  h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
 }
 for(unsigned i=0;i<32;i++)out[i]=(uint8_t)(h[i/4]>>(24-8*(i%4)));
}
bool hfw_catalog_valid(const struct hfw_entry *e,unsigned n) {
 if(!e || !n || n>HFW_MAP_CAP)return false;
 for(unsigned i=0;i<n;i++) {
  if(!e[i].length || e[i].base>UINT64_MAX-e[i].length || e[i].type>8)return false;
  for(unsigned j=0;j<i;j++)if(e[i].base<e[j].base+e[j].length && e[j].base<e[i].base+e[i].length)return false;
 }
 return true;
}
bool hfw_table_covered(const struct hfw_entry *e,unsigned n,uint64_t pa) {
 if(!pa || pa>UINT64_MAX-HFW_DMAR_BYTES || !hfw_catalog_valid(e,n))return false;
 unsigned overlaps=0;bool covered=false;
 for(unsigned i=0;i<n;i++)if(e[i].base<pa+HFW_DMAR_BYTES && pa<e[i].base+e[i].length) {
  overlaps++;covered=e[i].base<=pa && pa+HFW_DMAR_BYTES<=e[i].base+e[i].length && (e[i].type==2 || e[i].type==3);
 }
 return overlaps==1 && covered;
}
bool hfw_capture(struct hfw_result *r,const struct hfw_io *io,const uint8_t pin[32]) {
 if(!r)return false;
 *r=(struct hfw_result){.status=2};
 if(!io || !pin || !io->identity || !io->catalog || !io->table || !io->now)return false;
 const uint32_t expected[5]={0x0a268086,0x03000009,0x011a106b,0x0a048086,0x9c438086};
 for(unsigned p=0;p<2;p++) {
  struct hfw_sample *s=&r->sample[p];if(io->progress)io->progress(io->ctx,p);
  s->begin=io->now(io->ctx);
  if(!io->identity(io->ctx,s->identity))return false;
  for(unsigned i=0;i<5;i++)if(s->identity[i]!=expected[i])return false;
  r->status=3;
  if(!io->catalog(io->ctx,s->entries,&s->count) || !hfw_catalog_valid(s->entries,s->count))return false;
  r->status=4;
  if(!io->table(io->ctx,s->entries,s->count,s->dmar,&s->table_pa) || !hfw_table_covered(s->entries,s->count,s->table_pa))return false;
  unsigned sum=0;for(unsigned i=0;i<HFW_DMAR_BYTES;i++)sum+=s->dmar[i];
  if(s->dmar[0]!='D' || s->dmar[1]!='M' || s->dmar[2]!='A' || s->dmar[3]!='R' || s->dmar[4]!=136 || s->dmar[5] || s->dmar[6] || s->dmar[7] || s->dmar[8]!=1 || (sum&255))return false;
  uint8_t hash[32];hfw_sha256(s->dmar,hash);for(unsigned i=0;i<32;i++)if(hash[i]!=pin[i])return false;
  r->status=5;
  if(p) {
   const struct hfw_sample *a=&r->sample[0];
   if(s->count!=a->count || s->table_pa!=a->table_pa)return false;
   for(unsigned i=0;i<HFW_DMAR_BYTES;i++)if(s->dmar[i]!=a->dmar[i])return false;
   for(unsigned i=0;i<s->count;i++)if(s->entries[i].base!=a->entries[i].base || s->entries[i].length!=a->entries[i].length || s->entries[i].type!=a->entries[i].type)return false;
  }
  s->end=io->now(io->ctx);r->status=6;
  if(!s->begin || s->end<=s->begin || s->end-s->begin>100000 || (p && (s->begin<r->sample[0].end || s->begin-r->sample[0].end>100000)))return false;
 }
 r->status=1;return true;
}
