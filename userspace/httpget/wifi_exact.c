#include "wifi_exact.h"
#include "../wifihttpprobe/http_exact.h"
#include <srvros/sys.h>
#include <stddef.h>

struct probe_clock {uint64_t prepared,last,deadline;unsigned checks;int failed;};
static int check(struct probe_clock *c) {
 if(c->failed)return 0;
 if(c->checks==WIFI_EXACT_CHECK_CAP){c->failed=WIFI_EXACT_CHECKS;return 0;}
 c->checks++;
 long raw=srv_monotonic_us();
 if(raw<0 || (uint64_t)raw<c->last){c->failed=WIFI_EXACT_CLOCK;return 0;}
 c->last=(uint64_t)raw;
 if(c->last>=c->deadline){c->failed=WIFI_EXACT_DEADLINE;return 0;}
 return 1;
}
static int yield_checked(struct probe_clock *c) {
 if(!check(c))return 0;
 srv_yield();return check(c);
}
/* PREGO only: park for one real timer tick instead of consuming the shared
 * finite check budget while RF/bootstrap work runs. Original GO deadline/live
 * loops remain unchanged. Checks cannot bound time inside SYS_SLEEP itself. */
static int wait_go_checked(struct probe_clock *c) {
 if(!check(c))return 0;
 long result=srv_sleep_ticks(1);
 if(!check(c))return 0;
 if(result!=0){c->failed=WIFI_EXACT_SLEEP;return 0;}
 return 1;
}
static uint64_t le(const uint8_t *p,unsigned n) {
 uint64_t value=0;for(unsigned i=0;i<n;i++)value|=(uint64_t)p[i]<<(8*i);return value;
}
static int go(const uint8_t *p,long pid,struct probe_clock *c,uint32_t *ip) {
 static const uint8_t magic[8]={'S','R','V','R','H','T','P','1'};
 for(unsigned i=0;i<8;i++)if(p[i]!=magic[i])return 0;
 if(le(p+8,4)!=1 || le(p+12,4)!=WIFI_EXACT_GO_BYTES || le(p+16,8)!=(uint64_t)pid ||
    !le(p+24,8) || !le(p+32,8) || !le(p+40,8) || le(p+68,2)!=80)return 0;
 for(unsigned i=70;i<WIFI_EXACT_GO_BYTES;i++)if(p[i])return 0;
 uint64_t start=le(p+48,8),end=le(p+56,8);uint32_t address=(uint32_t)le(p+64,4);
 if(start<c->prepared || start>c->last || start>INT64_MAX-WIFI_EXACT_WINDOW_US ||
    end!=start+WIFI_EXACT_WINDOW_US || !address || (address>>24)==0 ||
    (address>>24)==127 || (address>>24)>=224)return 0;
 *ip=address;c->deadline=end;return 1;
}
int wifi_exact_main(int argc,char **argv) {
 if(argc!=3 || !argv || !argv[1] || !argv[2] ||
    argv[2][0]!='3' || argv[2][1])return WIFI_EXACT_ARGS;
 static const char exact[]="--wifi-exact",preflight[]="--wifi-preflight";
 unsigned i=0;while(exact[i] && argv[1][i]==exact[i])i++;
 int dry=0;
 if(argv[1][i]!=exact[i]) {
  i=0;while(preflight[i] && argv[1][i]==preflight[i])i++;
  if(argv[1][i]!=preflight[i])return WIFI_EXACT_ARGS;
  dry=1;
 }
 char request[HTTP_EXACT_REQUEST_CAPACITY];struct http_exact_parser parser;
 uint8_t record[WIFI_EXACT_GO_BYTES],buffer[512];size_t request_length=0;
 long initial=srv_monotonic_us();
 if(initial<0 || (uint64_t)initial>INT64_MAX-WIFI_EXACT_GO_WAIT_US)return WIFI_EXACT_CLOCK;
 struct probe_clock c={.prepared=(uint64_t)initial,.last=(uint64_t)initial,
     .deadline=(uint64_t)initial+WIFI_EXACT_GO_WAIT_US,.checks=1};
 if(!http_exact_build_request(request,sizeof request,&request_length) ||
    request_length!=HTTP_EXACT_REQUEST_BYTES)return WIFI_EXACT_PREP;
 http_exact_init(&parser);
 if(!check(&c))return c.failed;
 long version=srv_socket_try_io(-1,SRV_SOCKET_TCP_SEND_TRY,NULL,0,NULL);
 if(!check(&c))return c.failed;
 if(version!=SRV_SOCKET_TCP_TRY_ABI_VERSION)return WIFI_EXACT_ABI;
 if(!check(&c))return c.failed;
 long flags=srv_fcntl(3,SRV_F_GETFL,0);
 if(!check(&c))return c.failed;
 if(flags<0 || ((uint64_t)flags&(SRV_FD_READ|SRV_FD_WRITE|SRV_FD_NONBLOCK))!=
    (SRV_FD_READ|SRV_FD_NONBLOCK))return WIFI_EXACT_FD;
 if(!check(&c))return c.failed;
 long pid=srv_getpid();
 if(!check(&c))return c.failed;
 if(pid<=0)return WIFI_EXACT_FD;
 if(!check(&c))return c.failed;
 long ready=srv_syscall0(SYS_PROC_READY);
 if(!check(&c))return c.failed;
 if(ready!=0)return WIFI_EXACT_READY;
 size_t received=0;
 while(received<sizeof record) {
  if(!check(&c))return c.failed;
  long n=srv_socket_try_io(3,SRV_SOCKET_RECV,record+received,sizeof record-received,NULL);
  if(!check(&c))return c.failed;
  if(n==SRV_ERR_AGAIN){if(!wait_go_checked(&c))return c.failed;continue;}
  if(n<=0 || (uint64_t)n>sizeof record-received)return WIFI_EXACT_PIPE;
  received+=(size_t)n;
 }
 uint32_t ip=0;
 if(!go(record,pid,&c,&ip))return WIFI_EXACT_GO;
 /* The original window now bounds waiting for pipe EOF as well as TCP. */
 for(;;) {
  if(!check(&c))return c.failed;
  long n=srv_socket_try_io(3,SRV_SOCKET_RECV,buffer,1,NULL);
  if(!check(&c))return c.failed;
  if(n==SRV_ERR_AGAIN){if(!yield_checked(&c))return c.failed;continue;}
  if(n!=0)return WIFI_EXACT_PIPE;
  break;
 }
 if(!check(&c))return c.failed;
 long pipe_closed=srv_close(3);
 if(!check(&c))return c.failed;
 if(pipe_closed!=0)return WIFI_EXACT_CLOSE;
 if(!check(&c))return c.failed;
 /* Guest-only process/pipe preflight ends before any socket operation. */
 if(dry)return WIFI_EXACT_OK;
 /* Prelive ABI query succeeded; no extra live query between clock and connect.
  * A matched kernel cannot be replaced in this prepared process lifetime. */
 long native=srv_net_connect(ip,80,SRV_FD_NONBLOCK|SRV_NET_CONNECT_CACHE_ONLY);
 if(!check(&c))return c.failed;
 if(native<0 || native>INT32_MAX)return WIFI_EXACT_CONNECT;
 int fd=(int)native;
 for(;;) {
  if(!check(&c))return c.failed;
  long status=srv_net_error(fd);
  if(!check(&c))return c.failed;
  if(status==SRV_NETERR_NONE)break;
  if(status!=SRV_NETERR_INPROGRESS)return WIFI_EXACT_STATUS;
  if(!yield_checked(&c))return c.failed;
 }
 for(size_t sent=0;sent<request_length;) {
  if(!check(&c))return c.failed;
  long n=srv_net_respond_try(fd,request+sent,request_length-sent);
  if(!check(&c))return c.failed;
  if(n==SRV_ERR_AGAIN){if(!yield_checked(&c))return c.failed;continue;}
  if(n<=0 || (uint64_t)n>request_length-sent || (uint64_t)n>SRV_SOCKET_TCP_TRY_COPY_MAX)return WIFI_EXACT_SEND;
  sent+=(size_t)n;
 }
 for(;;) {
  if(!check(&c))return c.failed;
  long n=srv_socket_try_io(fd,SRV_SOCKET_RECV,buffer,sizeof buffer,NULL);
  if(!check(&c))return c.failed;
  if(n==SRV_ERR_AGAIN){if(!yield_checked(&c))return c.failed;continue;}
  if(n<0 || (uint64_t)n>sizeof buffer)return WIFI_EXACT_RECV;
  if(n) {if(http_exact_feed(&parser,buffer,(size_t)n)==HTTP_EXACT_ERROR)return WIFI_EXACT_HTTP;continue;}
  if(!check(&c))return c.failed;
  long status=srv_net_error(fd);
  if(!check(&c))return c.failed;
  if(status!=SRV_NETERR_NONE)return WIFI_EXACT_STATUS;
  if(http_exact_eof(&parser)!=HTTP_EXACT_COMPLETE)return WIFI_EXACT_HTTP;
  break;
 }
 if(!check(&c))return c.failed;
 long closed=srv_close(fd);
 if(!check(&c))return c.failed;
 return closed==0?WIFI_EXACT_OK:WIFI_EXACT_CLOSE;
}
