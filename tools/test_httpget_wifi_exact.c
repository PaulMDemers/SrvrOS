#define _POSIX_C_SOURCE 200809L
/* Actual httpget dispatch/mode/parser/SDK wrappers; strong HOST raw-call model.
 * No ELF/process/IRQ/kernel transport/native acceptance. */
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <srvros/sys.h>
#define main httpget_dispatch_main
#define getaddrinfo forbidden_getaddrinfo
#include "../userspace/httpget/httpget.c"
#undef getaddrinfo
#undef main
#include "../userspace/wifihttpprobe/http_exact.h"
static const char *scenario;
static uint64_t now,previous,prepared,deadline;
static unsigned clocks,queries,flags_calls,ready_calls,yields,connects,sends,receives,pipe_closes,net_closes,feed_calls;
static bool frozen,clock_failed,built,parser_ready,pipe_closed,eof_seen;
static uint8_t record[96],response[5000];static char request_bytes[4160];
static size_t pipe_at,response_at,response_size,request_at;
static int is(const char *s){return !strcmp(scenario,s);}
static void put(uint8_t *p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static void prepare_record(void){
 memset(record,0,sizeof record);memcpy(record,"SRVRHTP1",8);put(record+8,1,4);put(record+12,96,4);
 put(record+16,17,8);put(record+24,11,8);put(record+32,22,8);put(record+40,33,8);
 uint64_t start=now;if(is("stale-go"))start=prepared-1;if(is("future-go"))start+=1000000;
 put(record+48,start,8);deadline=start+15000000;put(record+56,deadline,8);put(record+64,0xc0000201,4);put(record+68,80,2);
 if(is("bad-pid"))record[16]++;if(is("bad-version"))record[8]=2;if(is("bad-size"))record[12]=95;
 if(is("zero-epoch"))memset(record+40,0,8);if(is("reserved"))record[95]=1;
 if(is("bad-span"))record[56]++;if(is("bad-ip"))put(record+64,0xe0000001,4);if(is("bad-port"))record[68]=81;
}
static void nonclock(void){assert(!clock_failed);}
long srv_syscall0(long op){
 if(op==SYS_MONOTONIC_US){
  clocks++;if(!frozen)now+=100;
  long value=is("negative-clock")?-1:(long)now;
  if(!prepared && value>=0)prepared=(uint64_t)value;
  if(value<0 || (uint64_t)value<previous || (deadline && pipe_at==96 && (uint64_t)value>=deadline) ||
    (!deadline && prepared && (uint64_t)value>=prepared+60000000))clock_failed=true;
  if(value>=0)previous=(uint64_t)value;return value;
 }
 nonclock();
 if(op==SYS_GETPID)return 17;
 if(op==SYS_PROC_READY){assert(built && parser_ready && queries==1 && flags_calls==1 && !ready_calls);ready_calls++;prepare_record();
  if(is("rollback-ready"))now-=1000;if(is("late-ready"))now=prepared+60000000;return is("ready-error")?-1:0;}
 if(op==SYS_YIELD){yields++;if(is("missing-pipe-eof"))now+=1000000;return 0;}
 abort();
}
long srv_syscall1(long op,long fd){nonclock();
 if(op==SYS_NET_ERROR){assert(fd==4);if(is("status-error"))return SRV_NETERR_RESET;if(is("late-status"))now=deadline;if(eof_seen && is("eof-error"))return SRV_NETERR_RESET;return 0;}
 assert(op==SYS_CLOSE);
 if(!pipe_closed){assert(fd==3);pipe_closes++;pipe_closed=true;return is("pipe-close-error")?-1:0;}
 assert(fd==4);net_closes++;if(is("late-close"))now=deadline;return is("close-error")?-1:0;
}
long srv_syscall3(long op,long a,long b,long c){nonclock();
 if(op==SYS_FCNTL){assert(a==3 && b==SRV_F_GETFL && !c);flags_calls++;return is("bad-fd-flags")?SRV_FD_READ:SRV_FD_READ|SRV_FD_NONBLOCK;}
 if(op==SYS_NET_CONNECT){assert(pipe_closed && !connects && (uint32_t)a==0xc0000201 && b==80 && c==(SRV_FD_NONBLOCK|SRV_NET_CONNECT_CACHE_ONLY));
  connects++;if(is("late-connect"))now=deadline;return is("connect-again")?SRV_ERR_AGAIN:is("connect-error")?-1:4;}
 if(op==SYS_NET_ERROR){assert(a==4);if(is("status-error"))return SRV_NETERR_RESET;
  if(is("late-status"))now=deadline;
  if(eof_seen && is("eof-error"))return SRV_NETERR_RESET;
  return 0;}
 abort();
}
long srv_syscall5(long op,long fd,long operation,long ptr,long length,long peer){nonclock();assert(op==SYS_SOCKET_TRY_IO && !peer);
 if(operation==SRV_SOCKET_TCP_SEND_TRY && fd==-1){assert(!ptr && !length);queries++;return is("old-abi")?SRV_ERR_INVAL:1;}
 if(operation==SRV_SOCKET_TCP_SEND_TRY){assert(fd==4 && pipe_closed && length>0);sends++;
  if(is("send-error"))return -1;if(is("send-zero"))return 0;if(is("send-oversized"))return 1537;
  if(is("send-again") && sends==1)return SRV_ERR_AGAIN;
  size_t n=(size_t)length;if(n>71)n=71;assert(!memcmp((void *)ptr,request_bytes+request_at,n));request_at+=n;
  if(is("rollback-send"))now-=1000;return (long)n;}
 assert(operation==SRV_SOCKET_RECV && length>0);
 if(!pipe_closed){assert(fd==3 && ready_calls);if(is("frozen-no-go"))return SRV_ERR_AGAIN;
  if(pipe_at==96){if(is("extra-pipe")){*(uint8_t *)ptr=1;return 1;}if(is("missing-pipe-eof"))return SRV_ERR_AGAIN;return 0;}
  if(is("short-pipe") && pipe_at>=42)return 0;
  size_t n=(size_t)length;if(n>7)n=7;if(n>96-pipe_at)n=96-pipe_at;memcpy((void *)ptr,record+pipe_at,n);pipe_at+=n;return (long)n;}
 assert(fd==4 && request_at==4159);receives++;
 if(is("recv-error"))return -1;
 if(is("recv-again") && receives==1)return SRV_ERR_AGAIN;
 if(response_at==response_size){if(is("frozen-no-eof"))return SRV_ERR_AGAIN;eof_seen=true;return 0;}
 size_t n=(size_t)length;if(n>13)n=13;if(n>response_size-response_at)n=response_size-response_at;
 memcpy((void *)ptr,response+response_at,n);response_at+=n;if(is("late-read"))now=deadline;return (long)n;
}
int forbidden_getaddrinfo(const char *a,const char *b,const struct addrinfo *c,struct addrinfo **d){(void)a;(void)b;(void)c;(void)d;abort();}
int __real_http_exact_build_request(char *,size_t,size_t *);
void __real_http_exact_init(struct http_exact_parser *);
enum http_exact_result __real_http_exact_feed(struct http_exact_parser *,const void *,size_t);
int __wrap_http_exact_build_request(char *p,size_t n,size_t *out){assert(!ready_calls && !built);int ok=__real_http_exact_build_request(p,n,out);assert(ok && *out==4159);assert(!memcmp(p,"GET /base64/",12));assert(!strcmp(p+4108," HTTP/1.0\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n"));
 const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 for(unsigned at=12,index=0;at<4108;at+=4){unsigned v=0;for(unsigned j=0;j<4;j++){const char *d=strchr(alphabet,p[at+j]);assert(d && p[at+j]);v=v*64+(unsigned)(d-alphabet);}for(unsigned j=0;j<3;j++,index++)assert(((v>>(16-j*8))&255)==(unsigned)('A'+index%26));}
 memcpy(request_bytes,p,4160);built=true;return ok;}
void __wrap_http_exact_init(struct http_exact_parser *p){assert(!ready_calls && !parser_ready);__real_http_exact_init(p);parser_ready=true;}
enum http_exact_result __wrap_http_exact_feed(struct http_exact_parser *p,const void *b,size_t n){feed_calls++;return __real_http_exact_feed(p,b,n);}
static int run(const char *s){
 scenario=s;now=1000000;previous=prepared=deadline=0;clocks=queries=flags_calls=ready_calls=yields=connects=sends=receives=pipe_closes=net_closes=feed_calls=0;
 frozen=is("frozen-no-go") || is("frozen-no-eof");clock_failed=built=parser_ready=pipe_closed=eof_seen=false;pipe_at=response_at=request_at=0;
 const char *header=is("duplicate-length")?"HTTP/1.1 200 OK\r\nContent-Length: 3072\r\nContent-Length: 3072\r\n\r\n":"HTTP/1.1 200 OK\r\nContent-Length: 3072\r\n\r\n";
 response_size=strlen(header);memcpy(response,header,response_size);
 for(unsigned i=0;i<3072;i++)response[response_size++]=(uint8_t)('A'+i%26);
 if(is("wrong-body"))response[strlen(header)+39]^=1;
 if(is("short-body"))response_size--;
 if(is("extra-body"))response[response_size++]=1;
 char *args[]={"httpget",is("preflight")?"--wifi-preflight":"--wifi-exact","3",NULL};int result=httpget_dispatch_main(3,args);
 assert(clocks<=WIFI_EXACT_CHECK_CAP && queries<=1 && connects<=1 && net_closes<=1);
 if(is("preflight")){assert(result==0 && pipe_closes==1 && !connects && !sends && !receives && !net_closes && !feed_calls);return result;}
 if(result==0)assert(request_at==4159 && eof_seen && net_closes==1 && pipe_closes==1 && feed_calls);
 if(is("late-read"))assert(!feed_calls);
 if(result && is("send-error"))assert(sends==1);
 return result;
}
int main(void){
 assert(wifi_exact_main(0,NULL)==WIFI_EXACT_ARGS);
 char *bad[]={"httpget","--wifi-exact",NULL};assert(httpget_dispatch_main(2,bad)==WIFI_EXACT_ARGS);
 struct {const char *name;int result;} cases[]={
 {"preflight",0},{"good",0},{"send-again",0},{"recv-again",0},{"old-abi",11},{"bad-fd-flags",12},{"negative-clock",13},
 {"rollback-ready",13},{"late-ready",24},{"ready-error",14},{"short-pipe",15},{"extra-pipe",15},
 {"missing-pipe-eof",24},{"bad-pid",16},{"bad-version",16},{"bad-size",16},{"zero-epoch",16},{"reserved",16},
 {"stale-go",16},{"future-go",16},{"bad-span",16},{"bad-ip",16},{"bad-port",16},{"connect-again",17},{"connect-error",17},
 {"late-connect",24},{"status-error",18},{"late-status",24},{"send-error",19},{"send-zero",19},{"send-oversized",19},
 {"rollback-send",13},{"recv-error",20},{"late-read",24},{"eof-error",18},{"wrong-body",21},{"duplicate-length",21},
 {"short-body",21},{"extra-body",21},{"pipe-close-error",22},{"close-error",22},{"late-close",24},
 {"frozen-no-go",23},{"frozen-no-eof",23}};
 for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++){int got=run(cases[i].name);if(got!=cases[i].result)fprintf(stderr,"%s got%d expected%d\n",cases[i].name,got,cases[i].result);assert(got==cases[i].result);}
 printf("httpget-wifi-exact: %zu HOST raw-syscall cases PASS actual dispatch/app/parser/SDK; no guest or native acceptance\n",sizeof cases/sizeof cases[0]);return 0;
}
