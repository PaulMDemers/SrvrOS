#ifdef NDEBUG
#undef NDEBUG
#endif
#define AIR61_READ_WINDOW_FIXTURE
#define SRVROS_AIR61_READ_WINDOW
#define WINDOW_PROFILE_FIXTURE_LIBRARY
#include "test_air61_window_profile.c"
extern bool air61_read_gate_check0(unsigned),air61_read_gate_check1(unsigned);
extern bool air61_read_gate_ram0(uint64_t),air61_read_gate_ram1(uint64_t);
extern void air61_read_gate_reset0(void),air61_read_gate_reset1(void);
extern const struct ecam_boot_gate_result *air61_read_gate_result0(void),*air61_read_gate_result1(void);
static unsigned read_phase_hook_mode;
void air61_read_test_phase(unsigned pass,unsigned phase){
 eg_phase=phase;eg.phases=phase?1:0;
 if(pass && !phase && read_phase_hook_mode){
  if(read_phase_hook_mode==1)cr0_value^=2;
  if(read_phase_hook_mode==2)map_entries[127].type=2;
  read_phase_hook_mode=0;
 }
}
static bool gates_equal(void){
 const struct ecam_boot_gate_result *a=air61_read_gate_result0(),*b=air61_read_gate_result1();
 const struct hfw_sample *x=&a->firmware[0].sample[0],*y=&b->firmware[0].sample[0];
 if(!x86_cache_equal(&a->cache[0],&b->cache[0]) || a->efer[0]!=b->efer[0] || a->mcfg_pa[0]!=b->mcfg_pa[0] || x->count!=y->count || x->table_pa!=y->table_pa)return false;
 if(memcmp(x->identity,y->identity,sizeof(x->identity)) || memcmp(x->dmar,y->dmar,136) || memcmp(a->mcfg[0],b->mcfg[0],60))return false;
 for(unsigned i=0;i<x->count;i++)if(x->entries[i].base!=y->entries[i].base || x->entries[i].length!=y->entries[i].length || x->entries[i].type!=y->entries[i].type)return false;
 const struct ecam_boot_policy *p=a->policy,*q=b->policy;
 if(p->extended_max!=q->extended_max || p->nx_edx!=q->nx_edx || p->apic!=q->apic || p->present_mask!=q->present_mask || p->launch_mask!=q->launch_mask || p->cr4!=q->cr4 || p->revision!=q->revision || p->flags!=q->flags || p->count!=q->count || p->bsp!=q->bsp || memcmp(p->ids,q->ids,sizeof(p->ids)))return false;
 return true;
}
static int inject_pass=-1,inject_phase=-1;
static unsigned inject_kind,phase_calls[2][3];
static int path_pass=-1;static unsigned path_level,path_bit;
static struct air61_read_pass_result retained;
static bool retained_valid;
static bool rcheck(void *ctx,unsigned pass,unsigned phase){
 (void)ctx;assert(pass<2 && phase<3);
 phase_calls[pass][phase]++;
 if(pass && !phase){retained=rw.pass[0].view;retained_valid=true;}
 if((int)pass==inject_pass && (int)phase==inject_phase){
  inject_pass=-1;
  switch(inject_kind){
   case 0:return false;
   case 1:cr3+=4096;break;
   case 2:irq_flags|=512;break;
   case 3:rw.pass[pass].view.window.token.epoch++;break;
   case 4:rw.pass[pass].view.window.words=65536;break;
   case 5:ticks+=1600000;break;
   case 6:assert(!vmm_air61_read_window_qualify(&rw.io,1,1));break;
   case 7:changed_cache=true;break;
   case 8:pages[0][384]^=32;break;
   case 9:pages[1][8]^=64;break;
   case 10:if(rw.pass[pass].owned[1])pages[7][80]^=64;else return false;break;
  }
 }
 if((int)pass==path_pass && phase==2 && rw.pass[pass].view.window.state==AIR61_WINDOW_ACTIVE){
  const struct air61_window_result *r=&rw.pass[pass].view.window;
  uint64_t pa[]={r->token.root,r->pdpt,r->frames[0],r->frames[1]};
  unsigned index[]={384,7,409,80};pages[frame_index(pa[path_level])][index[path_level]]^=1ull<<path_bit;path_pass=-1;
 }
 eg_phase=phase;eg.phases=phase?1:0; /* Existing hardware-model phase selector. */
 bool ok=pass?air61_read_gate_check1(phase):air61_read_gate_check0(phase);
 return ok && (!pass || phase || gates_equal());
}
static bool rram(void *ctx,unsigned pass,uint64_t pa){(void)ctx;assert(pass<2);return pass?air61_read_gate_ram1(pa):air61_read_gate_ram0(pa);}
static const struct air61_read_window_env rio={.check=rcheck,.ram=rram,.now=now};
static void rreset(void){
 setup();memset(&rw,0,sizeof(rw));air61_read_gate_reset0();air61_read_gate_reset1();
 pages[0][384]|=32;irq_flags=2;changed_catalog=false;
 read_phase_hook_mode=0;path_pass=-1;inject_pass=inject_phase=-1;inject_kind=0;memset(phase_calls,0,sizeof(phase_calls));retained_valid=false;
}
static bool run_series(void){return vmm_air61_read_window_qualify(&rio,0x61697272656164,1000);}
#ifdef AIR61_READ_WINDOW_LIBRARY
#define main read_backend_fixture_main
#endif
int main(void){
 rreset();assert(run_series());
 assert(rw.result.state==AIR61_READ_COMPLETE && rw.result.started==2 && rw.result.retired==2);
 for(unsigned pass=0;pass<2;pass++){
  const struct air61_read_pass_result *v=vmm_air61_read_window_pass(pass);
  const struct ecam_boot_gate_result *g=pass?air61_read_gate_result1():air61_read_gate_result0();
  assert(v->window.state==AIR61_WINDOW_RETIRED && !v->window.claimed && v->slots==12 && v->source_coalescings==1);
  assert(v->window.words<=65536 && v->window.end-v->window.begin<100000 && g->firmware_bytes==3996);
  assert(v->window.frees==2 && g->cache_reads[0]==49 && g->cache_reads[1]==49);
  assert(v->evidence.pt[80]==AIR61_READ_LEAF && v->evidence.pd[409]==(v->window.frames[1]|AIR61_READ_PARENT_FLAGS));
  for(unsigned i=0;i<12;i++)assert(v->offsets[i]==rw_offsets[i] && !memcmp(v->before[i],v->after[i],sizeof(v->before[i])));
  printf("read-pass=%u words=%u fw=%u slots=%u elapsed=%llu\n",pass,v->window.words,g->firmware_bytes,v->slots,(unsigned long long)(v->window.end-v->window.begin));
 }
 assert(allocs==4 && frees==4 && invalidates==4 && !pages[1][7]);
 assert(rw.result.gap_us>=20000 && rw.result.gap_us<=100000);
 assert(retained_valid && !memcmp(&retained,&rw.pass[0].view,sizeof(retained)));
 assert(!run_series());
 uint64_t denied=0;unsigned oldloads=loads;
 assert(!vmm_air61_read_window_copy_ram(0,0x2000000,&denied,8) && loads==oldloads);
 assert(!open_window());
 for(unsigned pass=0;pass<2;pass++)for(unsigned phase=0;phase<3;phase++)for(unsigned kind=0;kind<11;kind++){
  if(!phase && kind==9)continue; /* A changed initial unrelated entry is a fresh baseline. */
  rreset();inject_pass=(int)pass;inject_phase=(int)phase;inject_kind=kind;
  bool ok=run_series();if(ok)fprintf(stderr,"unexpected success pass=%u phase=%u kind=%u\n",pass,phase,kind);assert(!ok);assert(rw.result.fault && !rw.result.busy);
  unsigned nloads=loads,nstores=stores,nfrees=frees;
  assert(!run_series() && loads==nloads && stores==nstores && frees==nfrees);
  if(pass)assert(rw.pass[0].view.window.state==AIR61_WINDOW_RETIRED);
 }
 for(unsigned pass=0;pass<2;pass++)for(unsigned level=0;level<4;level++)for(unsigned bit=0;bit<64;bit++){
  rreset();path_pass=(int)pass;path_level=level;path_bit=bit;
  assert(!run_series() && rw.result.state==AIR61_READ_POISONED);
 }
 for(unsigned at=1;at<=4;at++){
  rreset();alloc_fail_at=at;assert(!run_series());
  assert(rw.result.state==AIR61_READ_REFUSED && !pages[1][7] && frees==at-1);
  rreset();free_fail_at=at;assert(!run_series() && rw.result.state==AIR61_READ_POISONED);
  rreset();invalidate_fail_at=at;assert(!run_series() && rw.result.state==AIR61_READ_POISONED);
 }
 rreset();for(unsigned i=5;i<128;i++){map_entries[i]=(struct limine_memmap_entry){.base=0x100000000ull+i*8192,.length=4096,.type=1};map_ptrs[i]=map_entries+i;}map_response.entry_count=128;
 assert(run_series());for(unsigned pass=0;pass<2;pass++)assert(rw.pass[pass].view.window.words<=65536);
 rreset();pages[0][384]&=~32ull;assert(!run_series() && !allocs && !stores);
 rreset();zero_clock=true;assert(!run_series() && !allocs && !stores);
 rreset();stop_clock=true;stopped_time=1000;assert(!run_series() && !allocs);
 rreset();delay_in_census=true;assert(!run_series() && !allocs);
 rreset();bad_pointer=true;assert(!run_series() && !allocs);
 rreset();duplicate=true;assert(!run_series() && rw.result.state==AIR61_READ_POISONED);
 rreset();delay_at_publish=true;assert(!run_series() && rw.result.state==AIR61_READ_POISONED);
 rreset();corrupt_parent=true;assert(!run_series() && rw.result.state==AIR61_READ_POISONED);
 rreset();corrupt_after_clear=true;assert(!run_series() && rw.result.state==AIR61_READ_POISONED);
 rreset();assert(open_window());assert(!run_series());
 rreset();assert(!vmm_air61_read_window_qualify(&rio,0,1000));
 assert(!vmm_air61_read_window_qualify(&rio,1,UINT64_MAX));
 assert(!rw.result.attempted && !loads);
 puts("air61-read-window PASS dormant actual VMM and two fresh gates; zero target loads");return 0;
}
