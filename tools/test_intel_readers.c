#include <srvros/intel_reader_audit.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static const char *mode;
static unsigned config_reads,plane_reads,guards;
static uint64_t time_us=10;
static bool is(const char *s){return !strcmp(mode,s);}
static uint64_t now(void *ctx){(void)ctx;if(is("zero"))return 0;if(is("frozen"))return 10;if(is("backward"))return --time_us;return ++time_us;}
static bool guard(void *ctx){(void)ctx;guards++;if(is("deadline"))time_us+=150001;return !is("guard") && !(is("late-guard") && plane_reads>3);}
static bool config(void *ctx,unsigned o,uint32_t *v) {
 (void)ctx;config_reads++;
 switch(o){case 0x34:*v=0x50;break;case 0x50:*v=0x26001;break;case 0x60:*v=0x7005;break;case 0x70:*v=0x00910010;break;case 0x74:*v=0x10000fc0;break;case 0x78:*v=0x800;break;default:assert(false);}
 if(is("pending") && o==0x78)*v|=1u<<21;
 if(is("status-reserved") && o==0x78)*v|=1u<<22;
 if(is("flr") && o==0x78)*v|=0x8000;
 if(is("capability") && o==0x70)*v=0x00920010;
 if(is("chain") && o==0x60)*v=0x7105;
 if(is("capabilities") && o==0x74)*v=0x10000fc1;
 if(is("cfg-drift") && config_reads>6 && o==0x78)*v=0;
 if(is("float-config"))*v=UINT32_MAX;
 return !is("read-config");
}
static bool plane(void *ctx,unsigned p,uint32_t w[INTEL_READER_WORDS]) {
 (void)ctx;assert(p<3);plane_reads++;memset(w,0,sizeof(uint32_t)*INTEL_READER_WORDS);if(!p)w[0]=1u<<31;
 if(is("active")){w[5]=1;w[8]=1u<<31;}
 if(is("mismatch"))w[3]=4096;
 if(is("changed") && plane_reads>3)w[4]=8192;
 if(is("float-plane"))w[12]=UINT32_MAX;
 return !is("read-plane");
}
int main(int argc,char **argv) {
 mode=argc>1?argv[1]:"normal";
 struct intel_reader_result r={0};struct intel_reader_io io={NULL,guard,config,plane,now};
 bool a=intel_reader_pass(&r,io,0),b=a && intel_reader_pass(&r,io,1),c=b && intel_reader_finish(&r,io);
 bool expected=is("normal") || is("active") || is("mismatch");assert(c==expected);
 if(expected){assert(r.complete && r.nonposted_clear && r.reads==90 && r.configs==2 && r.planes==6 && r.samples==2 && r.primary==1);assert(r.mismatch==(is("mismatch")?7u:0));assert(r.cursor==(is("active")?7u:0));}
 unsigned reads=config_reads+plane_reads;assert(!intel_reader_pass(&r,io,0) && !intel_reader_finish(&r,io));assert(reads==config_reads+plane_reads);
 struct intel_reader_result copy=r;assert(!intel_reader_pass(&copy,io,1) && !intel_reader_finish(&copy,io));assert(reads==config_reads+plane_reads);
 assert(!intel_reader_pass(NULL,io,0));
 printf("reader core %s PASS\n",mode);return 0;
}
