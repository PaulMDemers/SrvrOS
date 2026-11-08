/* Actual ECAM/admission/VMM/R2 producer. Device DWORDs are fixture-only. */
#define AIR61_REGISTER_SAMPLE_FIXTURE
#include "test_air61_read_combined.c"
#include "test_air61_sample_priming.inc"
static jmp_buf sample_halt;
static const char *sample_mode;
static unsigned calls;
void window_test_halt(void){longjmp(sample_halt,1);}
bool air61_sample_test_load(unsigned pass,unsigned slot,uint32_t offset,uint32_t *out){
 static const uint32_t data[12]={0,0x10,0,0x123456,0,0,0x40,0,0,0x2000000,0,0};
 static const uint32_t offsets[12]={0x1c,0,0xc,8,0xc,0x14,0x10,0x14,0x24,0x20,0x24,0x1c};
 assert(pass<2 && slot<12 && offset==offsets[slot] && calls==pass*12+slot);
 assert(!(irq_flags&512) && rw.current==pass && rw.pass[pass].view.window.claimed && rw.pass[pass].view.window.state==AIR61_WINDOW_ACTIVE);
 assert(text_reads==16 && gate_touches[0]>16 && gate_touches[1]>16);
 calls++;*out=data[slot];
 if(!strcmp(sample_mode,"partial") && slot==3)return false;
 if(!strcmp(sample_mode,"unavailable") && slot==3)*out=UINT32_MAX;
 if(!strcmp(sample_mode,"zero-ver") && slot==1)*out=0;
 if(!strcmp(sample_mode,"torn") && slot==4)*out^=1;
 if(!strcmp(sample_mode,"changed") && pass)*out^=1;
 if(!strcmp(sample_mode,"deadline") && slot==3)clock_ticks+=1600000;
 if(!strcmp(sample_mode,"root") && slot==3)cr3+=4096;
 if(!strcmp(sample_mode,"dirty") && slot==3)pages[7][80]|=64;
 return true;
}
int main(int argc,char **argv){
 sample_mode=argc>1?argv[1]:"normal";
 bool failure=!strcmp(sample_mode,"partial") || !strcmp(sample_mode,"unavailable") || !strcmp(sample_mode,"zero-ver") || !strcmp(sample_mode,"torn") || !strcmp(sample_mode,"changed") || !strcmp(sample_mode,"deadline") || !strcmp(sample_mode,"root") || !strcmp(sample_mode,"dirty");
 if(failure){
  if(!setjmp(sample_halt)){sample_producer_main(argc,argv);assert(false);}
  assert(rw.result.state==AIR61_READ_POISONED && !(irq_flags&512));
  unsigned old=calls;assert(!haswell_air61_register_sample() && calls==old);
  assert(!rw.result.retired || rw.result.retired==1);
  printf("sample-fixture: expected terminal poison calls=%u fault=%s\n",calls,rw.result.fault);return 0;
 }
 int result=sample_producer_main(argc,argv);assert(calls==24);return result;
}
