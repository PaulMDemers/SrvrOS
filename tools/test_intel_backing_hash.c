#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../kernel/src/drivers/intel_display_backing.c"
static uint64_t reference(uint64_t h,uint64_t word){
 unsigned char bytes[8];for(unsigned i=0;i<8;i++)bytes[i]=(unsigned char)(word>>(i*8));
 for(unsigned i=0;i<8;i++){h^=bytes[i];h*=UINT64_C(1099511628211);}return h;
}
static uint64_t random_word(uint64_t *state){*state^=*state<<13;*state^=*state>>7;*state^=*state<<17;return *state;}
static uint64_t ns(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
int main(int argc,char **argv){
 uint64_t state=0x91c27df934b02a51ull,hash=14695981039346656037ull;
 if(argc==2){
  bool fast=!strcmp(argv[1],"optimized");assert(fast || !strcmp(argv[1],"reference"));
  uint64_t vector[1356];for(unsigned i=0;i<1356;i++)vector[i]=0x90000001ull+4096ull*i;
  uint64_t begin=ns();
  if(fast)for(unsigned n=0;n<10000;n++)for(unsigned i=0;i<1356;i++)hash=backing_hash_word(hash,vector[i]);
  else for(unsigned n=0;n<10000;n++)for(unsigned i=0;i<1356;i++)hash=reference(hash,vector[i]);
  uint64_t elapsed=ns()-begin;printf("{\"mode\":\"%s\",\"ns\":%llu,\"hash\":%llu,\"words\":13560000}\n",argv[1],(unsigned long long)elapsed,(unsigned long long)hash);return 0;
 }
 for(unsigned bit=0;bit<64;bit++)for(unsigned seed=0;seed<4;seed++){
  uint64_t h=seed==0?0:seed==1?UINT64_MAX:seed==2?hash:random_word(&state);
  assert(backing_hash_word(h,1ull<<bit)==reference(h,1ull<<bit));
  assert(backing_hash_word(h,~(1ull<<bit))==reference(h,~(1ull<<bit)));
 }
 for(unsigned n=0;n<200000;n++){
  uint64_t h=random_word(&state),v=random_word(&state);
  assert(backing_hash_word(h,v)==reference(h,v));
  assert(backing_hash_word(h,v&UINT32_MAX)==reference(h,v&UINT32_MAX));
  assert(backing_hash_word(h,0)==reference(h,0));
  hash=backing_hash_word(hash,v);
 }
 uint64_t a=14695981039346656037ull,b=a;
 for(unsigned i=0;i<1356;i++){uint64_t v=0x90000001ull+4096ull*i;a=backing_hash_word(a,v);b=reference(b,v);assert(a==b);}
 puts("intel-backing-hash: PASS independent byte oracle, every bit/arbitrary seed,600k random/zero-upper/zero cases and every1356-entry prefix");
}
