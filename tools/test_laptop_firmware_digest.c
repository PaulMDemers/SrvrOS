/* Actual matcher/digest against the previous independent two-pass algorithm. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef SRVROS_LAPTOP_RESOURCES_FILE
#define SRVROS_LAPTOP_RESOURCES_FILE "../kernel/src/drivers/laptop_resources.c"
#endif
#include SRVROS_LAPTOP_RESOURCES_FILE
static uint8_t data[35034];
static volatile unsigned sink;
struct firmware_digest {uint64_t hash;uint8_t sum;};
static __attribute__((noinline)) struct firmware_digest reference(const uint8_t *b,size_t n,uint64_t seed) {
 struct firmware_digest d={seed,0};
 for(size_t i=0;i<n;i++){ d.hash=(d.hash^b[i])*1099511628211ull;d.sum+=b[i]; }
 return d;
}
static __attribute__((noinline)) bool fused_match(const uint8_t *b,size_t n) {
 if(n!=35034 || !table_header(b,n,"DSDT",36))return false;
 struct firmware_digest d=reference(b,n,14695981039346656037ull);
 return !d.sum && d.hash==0xe0f810cb41f75c0bull;
}
static __attribute__((noinline)) bool old_match(const uint8_t *b,size_t n) {
 if(!b || n<36 || n>1024*1024 || memcmp(b,"DSDT",4) || le32(b+4)!=n)return false;
 uint8_t sum=0;for(size_t i=0;i<n;i++)sum+=b[i];if(sum || n!=35034)return false;
 uint64_t h=14695981039346656037ull;for(size_t i=0;i<n;i++)h=(h^b[i])*1099511628211ull;
 return h==0xe0f810cb41f75c0bull;
}
static void check(size_t n) {
 if(n==sizeof(data)) {
  struct air_digest actual=air_firmware_digest(data);
  uint8_t sum=0;
  for(unsigned i=0;i<3;i++) {
   struct firmware_digest part=reference(data+11678*i,11678,air_hash_boundary[i]);
   assert(actual.hash[i]==part.hash);
   sum+=part.sum;
  }
  assert(actual.sum==sum);
 }
 assert(laptop_firmware_match(data,n)==old_match(data,n));
}
static uint64_t ns(void) {struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
int main(int argc,char **argv) {
 bool captured=argc>1,exhaustive=argc>2 && !strcmp(argv[2],"--exhaustive");
 if(captured) {
  FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(data,1,sizeof(data),f)==sizeof(data));assert(fgetc(f)==EOF);assert(!fclose(f));
  assert(old_match(data,sizeof(data)) && laptop_firmware_match(data,sizeof(data)));
 } else {
  for(unsigned i=0;i<sizeof(data);i++)data[i]=(uint8_t)(i*91+7);
  memcpy(data,"DSDT",4);uint32_t n=sizeof(data);for(unsigned i=0;i<4;i++)data[i+4]=(uint8_t)(n>>(8*i));
  struct firmware_digest d=reference(data,sizeof(data),14695981039346656037ull);data[9]-=d.sum;
  assert(!old_match(data,sizeof(data)) && !laptop_firmware_match(data,sizeof(data)));
 }
 assert(!laptop_firmware_match(NULL,0) && !laptop_firmware_match(data,0));
 check(sizeof(data));check(0);
 unsigned prefixes=0,bytes=0,pairs=0;
 for(size_t n=0;n<=sizeof(data);n+=exhaustive?1:137){check(n);prefixes++;}
 for(size_t i=0;i<sizeof(data);i+=exhaustive?1:131) {
  uint8_t old=data[i];data[i]^=(uint8_t)(1u<<(i%8));check(sizeof(data));
  if(captured)assert(!laptop_firmware_match(data,sizeof(data)));
  data[i]=old;bytes++;
 }
 for(size_t i=0;i<sizeof(data);i+=71) {
  size_t j=(i+193)%sizeof(data);data[i]++;data[j]--;check(sizeof(data));
  if(captured)assert(!laptop_firmware_match(data,sizeof(data)));
  data[i]--;data[j]++;pairs++;
 }
 for(unsigned i=0;i<8;i++) {
  uint8_t old=data[i];data[i]^=1;check(sizeof(data));assert(!laptop_firmware_match(data,sizeof(data)));data[i]=old;
 }
 assert(!laptop_firmware_match(data,35033) && !laptop_firmware_match(data,35035));
 printf("firmware-digest: PASS captured=%u prefixes=%u byte-mutations=%u checksum-preserving=%u exact-header-length-hash-checksum=1\n",captured,prefixes,bytes,pairs);
 bool (*volatile functions[2])(const uint8_t *,size_t)={fused_match,laptop_firmware_match};
 for(unsigned trial=0;trial<4;trial++)for(unsigned k=0;k<2;k++) {
  unsigned method=k^(trial&1);uint64_t begin=ns();
  for(unsigned i=0;i<512;i++)sink+=functions[method](data,sizeof(data));
  printf("firmware-host-timing: trial=%u method=%s ns_per_call=%llu native_acceptance=0\n",trial,method?"segmented":"fused",(unsigned long long)((ns()-begin)/512));
 }
 return 0;
}
