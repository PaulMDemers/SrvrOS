#include <srvros/wpa2_nonce.h>
#include <stddef.h>
struct nonce_state {uint8_t busy,failed,have;uint64_t generation,last[4];};
struct nonce_source {void *context;size_t context_size;bool (*supported)(void *);bool (*word)(void *,uint64_t *);};
static struct nonce_state nonce_state;
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static bool nonce_same(const struct nonce_state *s,uint64_t generation,uint8_t have,const uint64_t prior[4]){
 if(__atomic_load_n(&s->busy,__ATOMIC_ACQUIRE)!=1 || s->failed || s->generation!=generation || s->have!=have)return false;
 for(unsigned i=0;i<4;i++)if(s->last[i]!=prior[i])return false;
 return true;
}
/* Source callbacks are trusted,serialized and nonmutating with respect to
 * state/output. Context may hold host-model counters;it never aliases output. */
static bool nonce_fill(struct nonce_state *s,const struct nonce_source *io,uint8_t *out){
 if(!s || !io || !out || overlap(out,32,s,sizeof *s) || overlap(out,32,io,sizeof *io) ||
 (io->context && overlap(out,32,io->context,io->context_size)))return false;
 scrub(out,32);
 if(__atomic_test_and_set(&s->busy,__ATOMIC_ACQUIRE))return false;
 uint64_t words[4]={0},prior[4]={0},generation=s->generation;uint8_t have=s->have;
 for(unsigned i=0;i<4;i++)prior[i]=s->last[i];
 const struct nonce_source f=*io;
 bool ok=!s->failed && have<=1 && (have?generation>0:generation==0) && generation!=UINT64_MAX &&
 ((!f.context && !f.context_size) || (f.context && f.context_size)) && f.supported && f.word && f.supported(f.context);
 ok=ok && nonce_same(s,generation,have,prior);
 for(unsigned i=0;i<4 && ok;i++){
  bool read=false;
  for(unsigned n=0;n<10 && !read;n++){
   read=f.word(f.context,&words[i]);
   if(!nonce_same(s,generation,have,prior)){ok=false;break;}
  }
  ok=ok && read && words[i] && words[i]!=UINT64_MAX;
  for(unsigned j=0;j<i && ok;j++)if(words[i]==words[j])ok=false;
 }
 uint64_t different=0;for(unsigned i=0;i<4;i++)different|=words[i]^prior[i];
 ok=ok && (!have || different) && s->generation==generation && s->have==have && !s->failed;
 for(unsigned i=0;i<4 && ok;i++)if(s->last[i]!=prior[i])ok=false;
 if(ok){
  for(unsigned i=0;i<4;i++){s->last[i]=words[i];for(unsigned b=0;b<8;b++)out[8*i+b]=(uint8_t)(words[i]>>(8*b));}
  s->have=1;s->generation=generation+1;
 }else s->failed=1;
 scrub(words,sizeof words);scrub(prior,sizeof prior);
 __atomic_clear(&s->busy,__ATOMIC_RELEASE);return ok;
}
static bool nonce_supported(void *unused){
 (void)unused;
#if defined(__x86_64__)
 uint32_t a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0),"c"(0));
 if(a<1)return false;
 __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
 return !!(c&(1u<<30));
#else
 return false;
#endif
}
static bool nonce_word(void *unused,uint64_t *v){
 (void)unused;
#if defined(__x86_64__)
 uint8_t ok;__asm__ volatile("rdrand %0; setc %1":"=r"(*v),"=qm"(ok)::"cc");return !!ok;
#else
 (void)v;return false;
#endif
}
bool wpa2_nonce_generate(uint8_t out[32]){
 const struct nonce_source source={.supported=nonce_supported,.word=nonce_word};
 return nonce_fill(&nonce_state,&source,out);
}
