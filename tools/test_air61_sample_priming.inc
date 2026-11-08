/* Linker text/RAM first-touch model. No target read is used for priming. */
static const uint64_t sample_text_base=0xffffffff80100000ull;
static unsigned text_reads,gate_touches[2],text_fail_at,text_fault_kind;
static unsigned text_pages=16,text_fault_level,text_fault_bit;
void air61_sample_test_setup_text(void){
 pages[0][511]=0x2002023;pages[2][510]=0x2003023;pages[3][0]=0x2004023;
 for(unsigned i=0;i<16;i++)pages[4][256+i]=0x2000001;
 text_reads=gate_touches[0]=gate_touches[1]=text_fail_at=text_fault_kind=0;text_pages=16;
}
void air61_sample_test_text_bounds(uint64_t *begin,uint64_t *end){*begin=sample_text_base;*end=*begin+4096ull*text_pages;}
bool air61_sample_test_text_read(uint64_t va){
 assert(!rw.current && !rw.pass[0].snapshot && !rw.pass[0].in_census);
#ifdef AIR61_REGISTER_SAMPLE_FIXTURE
 assert(allocs==1);
#else
 assert(!allocs);
#endif
 assert(rw.pass[0].view.window.state==AIR61_WINDOW_CLAIMED);
 assert(va==sample_text_base+4096ull*text_reads && text_reads<16);
 text_reads++;pages[4][256+(unsigned)((va-sample_text_base)/4096)]|=32;
 if(text_reads==text_fail_at){
  if(text_fault_kind==0)return false;
  if(text_fault_kind==1)cr3+=4096;
  if(text_fault_kind==2)irq_flags|=512;
#ifdef AIR61_REGISTER_SAMPLE_FIXTURE
  if(text_fault_kind==3)clock_ticks+=1600000;
#else
  if(text_fault_kind==3)ticks+=1600000;
#endif
  if(text_fault_kind==4)pages[4][256+(unsigned)((va-sample_text_base)/4096)]|=64;
  if(text_fault_kind==5){
   uint64_t *entry[]={&pages[0][511],&pages[2][510],&pages[3][0],&pages[4][256+(unsigned)((va-sample_text_base)/4096)]};
   *entry[text_fault_level]^=1ull<<text_fault_bit;
  }
 }
 return true;
}
void air61_sample_test_gate_touch(unsigned pass,const void *address){
 (void)address;assert(pass<2 && !rw.current && rw.pass[0].checking && !rw.pass[0].in_census && !rw.pass[0].snapshot);
 assert(!rw.pass[0].view.attempts && !rw.pass[0].view.reads);gate_touches[pass]++;
}
