/* Actual submit + command completion cores, synthetic private ownership. */
#include <srvros/intel_bdw_bcs_flush.h>
#include <srvros/intel_bdw_bcs_submit.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct intel_bdw_bcs_flush flush;
static struct bcs_flush_source source;
static struct intel_bdw_bcs_submit port;
static struct bcs_submit_source submission;
static unsigned owner,lifetime,context,cases;
enum fault {NORMAL,FAIL_PROOF,WRITE_NONE,WRITE_UNKNOWN,LOST_REQUIREMENT,SLOW_PORT,WRONG_NONCE,STALE_NONCE,AGED};
static struct {
 enum fault fault;unsigned at,signal,polls,reads,publishes,ports,stores,triggered;
 uint64_t time;bool held,visible;
 uint32_t packet[6];
} m;
static const uint64_t nonce=0x1122334455667788ull;
static uint64_t now(void *ctx){assert(ctx==&m);return ++m.time;}
static bool original(void) {
 return m.held && m.visible && flush.object==&flush && flush.source==&source && source.object==&source && source.consumer==&flush &&
 flush.busy && !flush.retained && !flush.reason && !flush.complete && flush.p.state==BCS_FLUSH_SUBMITTING &&
 flush.p.published==1 && !flush.p.submitted && flush.p.submit_attempts==1 && m.packet[0]==0x13044003 && !m.packet[5];
}
static bool port_proof(void *ctx,const struct bcs_submit_source *s,unsigned prefix,struct bcs_submit_proof *p) {
 assert(ctx==&m && s==&submission && prefix==m.stores);
 if(!original() || (m.fault==FAIL_PROOF && prefix==m.at))return false;
 *p=(struct bcs_submit_proof){&submission,&owner,&m,&lifetime,&context,0x9000,7,19,0x000000170240201dull,m.time,BCS_SUBMIT_REQUIRED,prefix};
 if(m.fault==LOST_REQUIREMENT)p->requirements^=1u<<m.at;
 return true;
}
static enum intel_gt_write port_write(void *ctx,uint32_t off,uint32_t value) {
 assert(ctx==&m && original() && off==0x22230 && m.ports<4);
 const uint32_t words[]={0,0,23,0x0240201d};assert(value==words[m.ports]);++m.ports;
 if(m.ports==m.at && m.fault==WRITE_NONE)return GT_WRITE_NONE;
 if(m.ports==m.at && m.fault==WRITE_UNKNOWN)return GT_WRITE_UNCERTAIN;
 ++m.stores;if(m.stores==4)++m.triggered;
 if(m.fault==SLOW_PORT && m.ports==m.at)m.time=submission.binding.admitted_at+10000;
 return GT_WRITE_DONE;
}
static bool ownership(void *ctx,const struct bcs_flush_source *s,struct bcs_flush_proof *p) {
 assert(ctx==&m && s==&source);
 *p=(struct bcs_flush_proof){&source,&owner,&m,&lifetime,&context,7,19,0x9000,m.time,BCS_FLUSH_REQUIRED};
 return m.held;
}
static bool read_completion(void *ctx,uint64_t gpu,uint64_t *out) {
 assert(ctx==&m && gpu==0x2401000);++m.reads;
 if(m.fault==STALE_NONCE && !m.triggered){*out=nonce;return true;}
 *out=0;
 if(m.triggered) {
  ++m.polls;
  if(m.fault==WRONG_NONCE)*out=nonce^1;
  else if(m.polls>=m.signal)*out=nonce;
 }
 return true;
}
static enum intel_gt_write publish(void *ctx,uint64_t gpu,const uint32_t *p,unsigned count) {
 assert(ctx==&m && gpu==0x2400000 && count==6 && !m.publishes);
 memcpy(m.packet,p,24);++m.publishes;m.visible=true;return GT_WRITE_DONE;
}
static enum intel_gt_write submit(void *ctx,const void *c,uint64_t gpu,uint32_t tail) {
 assert(ctx==&m && c==&context && gpu==0x2400000 && tail==24 && original());
 submission.object=&submission;
 submission.binding=(struct bcs_submit_binding){&owner,&m,&lifetime,&context,0x9000,7,19,0x000000170240201dull,
  0x2400000,0x1000000,0x2402000,0x1002000,0x2401000,0x1001000,0x10000000,0x100000,0x2300000,
  1000000,1000000,1000000,1000000,flush.p.last,0x8086,0x1626,24};
 bool issued=intel_bdw_bcs_submit_run(&port,&submission,(struct bcs_submit_io){&m,port_proof,port_write,now});
 if(issued)return GT_WRITE_DONE;
 /* A partial FIFO or known trigger followed by lost verification is never
  * translated to definitely-not-issued or retried by the parent. */
 return port.p.writes || port.p.uncertain?GT_WRITE_UNCERTAIN:GT_WRITE_NONE;
}
static void pause_once(void *ctx){assert(ctx==&m);++m.time;}
static void run(enum fault fault,unsigned at,unsigned signal,bool expected) {
 memset(&flush,0,sizeof(flush));memset(&source,0,sizeof(source));memset(&port,0,sizeof(port));memset(&submission,0,sizeof(submission));
 memset(&m,0,sizeof(m));m.fault=fault;m.at=at;m.signal=signal;m.time=1000000;m.held=true;
 if(fault==AGED)m.time+=220000;
 source.object=&source;
 source.binding=(struct bcs_flush_binding){&owner,&m,&lifetime,&context,7,19,0x9000,nonce,
  0x2400000,0x1000000,0x2401000,0x1001000,0x10000000,0x100000,0x2300000,
  1000000,1000000,1000000,1000000,0x8086,0x1626};
 bool complete=intel_bdw_bcs_flush_run(&flush,&source,(struct bcs_flush_io){&m,ownership,read_completion,publish,submit,now,pause_once});++cases;
 assert(complete==expected && m.held && source.consumer==&flush);
 if(expected) {
  assert(port.issued && m.triggered==1 && m.stores==4 && m.ports==4 && m.polls==signal && flush.p.observed==nonce && flush.p.submitted==1);
  assert(!flush.retained && !port.retained && flush.p.polls>=1);
 } else {
  assert(flush.retained && !flush.complete);
  if(fault==FAIL_PROOF || fault==WRITE_NONE || fault==WRITE_UNKNOWN || fault==LOST_REQUIREMENT || fault==SLOW_PORT) {
   assert(port.retained && !port.issued && m.reads==1 && !m.polls && !flush.p.submitted);
   assert(flush.p.uncertain==(port.p.writes || port.p.uncertain));
  }
  if(fault==STALE_NONCE)assert(!m.publishes && !m.ports && !port.object);
  if(fault==AGED)assert(!m.publishes && !m.ports && !port.object);
  if(fault==NORMAL)assert(port.issued && m.triggered==1 && m.polls==256 && flush.p.submitted==1);
 }
 if(port.object)assert(submission.consumer==&port && port.p.writes==m.stores && port.p.attempts==m.ports && port.p.triggered==m.triggered);
}
int main(void) {
 for(unsigned n=1;n<=256;n*=2)run(NORMAL,0,n,true);
 run(NORMAL,0,257,false);
 for(unsigned n=0;n<=4;n++)run(FAIL_PROOF,n,1,false);
 for(unsigned n=1;n<=4;n++){run(WRITE_NONE,n,1,false);run(WRITE_UNKNOWN,n,1,false);run(SLOW_PORT,n,1,false);}
 for(unsigned n=0;n<5;n++)run(LOST_REQUIREMENT,n,1,false);
 run(WRONG_NONCE,0,1,false);run(STALE_NONCE,0,1,false);
 run(AGED,0,3,BCS_FLUSH_PREPARE_US==250000);
 printf("bcs-submit-flush: PASS cases=%u; actual joined cores, nonce completion distinct from port issuance, synthetic only\n",cases);
 return 0;
}
