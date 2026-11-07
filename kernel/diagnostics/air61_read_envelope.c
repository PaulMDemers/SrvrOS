/* Retained RAM receipt only, after both windows retire and logger quiesces. */
#include <srvros/air61_read_qualification.h>
#include <srvros/console.h>
#include <srvros/timer.h>
#include <stddef.h>
static uint8_t wire[AIR61_READ_RECEIPT_CAP];
static bool reported;static char saved_session[28];
static const char *read_session(const char *p){
 if(!p)return NULL;
 const char *s=p;for(;*p;p++)if(*p=='/')s=p+1;
 const char prefix[]="session-";for(unsigned i=0;i<8;i++)if(s[i]!=prefix[i])return NULL;
 for(unsigned i=8;i<27;i++)if(!s[i] || (i==24?s[i]!='-':!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f'))))return NULL;
 return s[27]?NULL:s;
}
bool haswell_air61_read_report(const char *directory){
 const char *s=read_session(directory);if(reported || !s)return false;
 reported=true;for(unsigned i=0;i<28;i++)saved_session[i]=s[i];
 const struct air61_read_series_result *r=vmm_air61_read_window_result();unsigned bytes=0,chunks=0;
 bool ok=haswell_air61_read_encode(s,wire,sizeof(wire),&bytes);
 console_printf("hsw-read: v=1 state=%u complete=%u session=%s epoch=%u root=%x\n",(uint64_t)r->state,(uint64_t)ok,s,r->epoch,r->root);
 static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 for(unsigned start=0;start<bytes;start+=512){
  unsigned length=bytes-start;if(length>512)length=512;char out[685];unsigned at=0;
  for(unsigned i=0;i<length;i+=3){unsigned n=length-i;uint32_t v=(uint32_t)wire[start+i]<<16;if(n>1)v|=(uint32_t)wire[start+i+1]<<8;if(n>2)v|=wire[start+i+2];out[at++]=alphabet[v>>18];out[at++]=alphabet[(v>>12)&63];out[at++]=n>1?alphabet[(v>>6)&63]:'=';out[at++]=n>2?alphabet[v&63]:'=';}
  out[at]=0;console_printf("hsw-read-data: v=1 chunk=%u data=%s\n",(uint64_t)chunks++,out);
 }
 console_printf("hsw-read-end: v=1 bytes=%u chunks=%u report=%u ok=%u target_reads=0 target_writes=0\n",(uint64_t)bytes,(uint64_t)chunks,timer_monotonic_us(),(uint64_t)ok);
 for(unsigned i=0;i<2;i++){const struct air61_read_pass_result *v=vmm_air61_read_window_pass(i);const struct air61_window_result *w=&v->window;
  console_printf("hsw-read-pass: pass=%u state=%u begin=%u end=%u words=%u slots=%u first=%u last=%u claimed=%u\n",(uint64_t)i,(uint64_t)w->state,w->begin,w->end,(uint64_t)w->words,(uint64_t)v->slots,v->slot_begin,v->slot_end,(uint64_t)w->claimed);
 }
 console_printf("hsw-read-failure: reason=%s\n",r->fault?r->fault:"none");return ok;
}
void haswell_air61_read_reference(const char *directory,bool saved){
 const char *s=read_session(directory);if(!s || !reported)return;
 for(unsigned i=0;i<28;i++)if(s[i]!=saved_session[i])return;
 const struct air61_read_series_result *r=vmm_air61_read_window_result();
 console_printf("hsw-read-ref: v=1 session=%s epoch=%u root=%x state=%u saved=%u target_reads=0 target_writes=0\n",s,r->epoch,r->root,(uint64_t)r->state,(uint64_t)saved);
}
