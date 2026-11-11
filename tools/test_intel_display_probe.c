#include <stdlib.h>
#ifdef SRVROS_INTEL_PREPARE_RESET
#include <srvros/intel_engine_held_reset.h>
static unsigned held_reset_boundary,held_reset_reads,held_reset_attempts,held_reset_writes,held_reset_domain_reads,held_reset_clock_fault;
static unsigned range_calls,range_fail_at;
static void intel_display_test_held_reset_boundary(bool);
static void intel_display_test_held_reset_admission(void);
static bool intel_display_test_held_reset_snapshot_read(unsigned,uint32_t,uint32_t *);
static bool intel_display_test_held_reset_domain(uint32_t *);
static enum intel_gt_write intel_display_test_held_reset_transition(uint32_t,uint32_t *);
#ifdef SRVROS_INTEL_HELD_INTERVAL
static unsigned interval_reads;
static bool intel_display_test_interval_read(uint32_t,uint32_t *);
static void intel_display_test_interval_admission(void);
#ifdef SRVROS_INTEL_REHOLD
static unsigned rehold_reads,rehold_stores;
static uint64_t rehold_owner_origin;
static void intel_display_test_rehold_source(void *);
static bool intel_display_test_rehold_read(unsigned,uint32_t,uint32_t *);
static void intel_display_test_rehold_admission(void);
static enum intel_gt_write intel_display_test_rehold_transition(uint32_t,uint32_t *);
#endif
#endif
#endif
#ifdef SRVROS_INTEL_HELD_REQUESTER
#include <srvros/intel_pci_held.h>
static unsigned held_reads,held_boundary;
static bool intel_display_test_held_read(uint32_t,uint32_t *);
static void intel_display_test_held_boundary(bool);
#endif
#ifdef SRVROS_INTEL_REQUESTER_AUDIT
#include <srvros/intel_pci_requester.h>
static unsigned requester_reads;
static bool intel_display_test_requester_read(unsigned,uint32_t,uint32_t *);
#endif
#ifdef SRVROS_INTEL_ENGINE_PREPARE
#include <srvros/intel_engine_prepare.h>
static uint32_t prepare_controls[5];
static unsigned prepare_stores,prepare_reader_calls;
static unsigned backing_batch_test_fault,backing_batch_test_reads,backing_batch_test_at;
static enum intel_gt_write intel_display_test_prepare_transition(unsigned,uint32_t,uint32_t *);
static bool intel_display_test_prepare_reader(uint32_t,uint32_t *);
#endif
#include <assert.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <srvros/intel_display_capture.h>
/* Each host case models a fresh boot; production supplies no reset entry. */
#include "../kernel/src/intel_boot_claim.c"
#include "../kernel/src/display_access.c"
#if defined(SRVROS_INTEL_RESET_READY_TRIAL) || defined(SRVROS_INTEL_BCS_RESET_TRIAL)
#include <srvros/intel_reset_ready.h>
static unsigned ready_writes,ready_reads,ready_group_words,ready_phase_reads;
static bool intel_display_test_reset_ready_read(uint32_t,uint32_t *);
static enum intel_gt_write intel_display_test_reset_ready_write(uint32_t,uint32_t);
#endif
#ifdef SRVROS_INTEL_COMPLETION_TRIAL
#include <srvros/intel_completion_audit.h>
static unsigned completion_context_reads;
static bool intel_display_test_completion_read(unsigned,unsigned,uint32_t,uint32_t *);
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
static void census_fault(unsigned,unsigned);
#endif
static bool intel_display_test_completion_context(unsigned,uint32_t *);
#endif
#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
static bool intel_display_test_wake_snapshot(uint32_t *,uint32_t *,uint32_t *);
#endif
#ifdef SRVROS_INTEL_BCS_RESET_TRIAL
static unsigned bcs_reset_writes,bcs_plane_calls,bcs_domain_observations,core_ack_clocks;
static uint64_t intel_display_test_bcs_now(void);
static bool intel_display_test_wake_snapshot(uint32_t *,uint32_t *,uint32_t *);
static bool intel_display_test_bcs_read(uint32_t,uint32_t *);
static enum intel_gt_write intel_display_test_bcs_write(uint32_t,uint32_t);
#endif
#ifdef SRVROS_INTEL_ENGINE_TRIAL
static const char *engine_mode="report";
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
static bool bcs_exit_fault(const char *mode) {
 const char *colon=strchr(engine_mode,':');
 unsigned n=colon?(unsigned)atoi(colon+1):0;
 return colon && !strncmp(engine_mode,mode,strlen(mode)) && n>=1 && n<=4 &&
  ready_writes+bcs_reset_writes==n;
}
static bool bcs_post_exit_fault(const char *mode) {
 const char *colon=strchr(engine_mode,':');
 unsigned n=colon?(unsigned)atoi(colon+1):0;
 return colon && !strncmp(engine_mode,mode,strlen(mode)) && n>=1 && n<=2 &&
  ready_writes==2 && bcs_reset_writes==2 && completion_context_reads==12+6*n;
}
#endif
#ifdef SRVROS_INTEL_ENGINE_PREPARE
static bool prepare_exit_fault(const char *mode) {
 const char *colon=strchr(engine_mode,':');
 return colon && !strncmp(engine_mode,mode,strlen(mode)) && prepare_stores==(unsigned)atoi(colon+1);
}
static bool prepare_post_exit_fault(const char *mode) {
 const char *colon=strchr(engine_mode,':');
 unsigned n=colon?(unsigned)atoi(colon+1):0;
 return colon && !strncmp(engine_mode,mode,strlen(mode)) && n>=1 && n<=2 &&
  prepare_stores==10 && completion_context_reads==12+6*n;
}
#endif
#endif
#ifdef SRVROS_INTEL_BLITTER_TRIAL
static bool bcs_entry_disabled;
static uint32_t bcs_ctl=0xf001;static unsigned bcs_reads,bcs_writes,reader_calls;
static bool intel_display_test_blitter_read(uint32_t *);
static enum intel_gt_write intel_display_test_blitter_write(uint32_t);
static bool intel_display_test_reader_read(unsigned,unsigned,uint32_t *);
#endif
static bool wake_mode,wake_cache_fail,wake_map_fail,wake_retire_fail,wake_uncertain,wake_no_ack,wake_foreign,wake_busy,wake_thread_stuck,wake_late_foreign,wake_fw_after;
static uint32_t wake_request __attribute__((unused))=0x10000,wake_ack __attribute__((unused));static unsigned wake_writes;static bool wake_irq __attribute__((unused));
#ifdef SRVROS_INTEL_WAKE_TRIAL
static uint64_t intel_display_test_wake_irq_save(void){wake_irq=true;return 512;}
static bool intel_display_test_wake_irq_off(void){return wake_irq;}
static void intel_display_test_wake_irq_restore(uint64_t flags){assert(flags==512);wake_irq=false;}
static bool intel_display_test_wake_uc(uint64_t p,uint32_t o){assert(p==0xc0000000ull+o && (o==0xa000 || o==0x130000 || o==0x138000
#ifdef SRVROS_INTEL_HELD_INTERVAL
 || o==0x811000
#endif
#ifdef SRVROS_INTEL_ENGINE_TRIAL

#ifdef SRVROS_INTEL_COMPLETION_TRIAL
 || o==0x4000
#ifdef SRVROS_INTEL_TAKEOVER_PREFLIGHT
 || o==0x9000
#endif
#if defined(SRVROS_INTEL_BCS_RESET_TRIAL) || defined(SRVROS_INTEL_ENGINE_PREPARE)
 || o==0x9000 || o==0x70000 || o==0x71000 || o==0x72000
#endif
#endif
#ifdef SRVROS_INTEL_BLITTER_TRIAL
 || o==0x70000 || o==0x71000 || o==0x72000
#endif
 || o==0x2000 || o==0x12000 || o==0x1a000 || o==0x1c000 || o==0x22000
#endif
));
#ifdef SRVROS_INTEL_ENGINE_TRIAL
 if(!strcmp(engine_mode,"cache-last") && o==0x22000)return false;
#ifdef SRVROS_INTEL_COMPLETION_TRIAL
 if(!strcmp(engine_mode,"tlb-cache") && o==0x4000)return false;
#endif
#endif
#ifdef SRVROS_INTEL_BCS_RESET_TRIAL
 if(!strcmp(engine_mode,"bcs-cache") && o==0x9000)return false;
#endif
#ifdef SRVROS_INTEL_PREPARE_RESET
 if(!strcmp(engine_mode,"hreset-cache-slot") && o==0x9000)return false;
#endif
#ifdef SRVROS_INTEL_HELD_INTERVAL
 if(!strcmp(engine_mode,"interval-cache-slot") && o==0x811000)return false;
#endif
 return !wake_cache_fail;}
static bool intel_display_test_wake_read(uint32_t reg,uint32_t *v){assert(wake_irq && !display_access_allowed());*v=reg==0xa188?wake_request:reg==0x130044?wake_ack:wake_thread_stuck?1:0;return true;}
static enum intel_gt_write intel_display_test_wake_write(uint32_t);
static void intel_display_test_wake_pause(void){}
#endif
#ifdef SRVROS_INTEL_ENGINE_TRIAL
static unsigned engine_calls;
static bool intel_display_test_engine_read(unsigned e,unsigned w,uint32_t *v) {
 assert(wake_irq && !display_access_allowed() && wake_ack==1 && (wake_request&1)==1);
 engine_calls++;
#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
 if(engine_calls>60 && !strcmp(engine_mode,"blit-post-read"))return false;
#endif
 if(!strcmp(engine_mode,"late-foreign"))wake_request|=2;
 *v=w==4?1:0;
#ifdef SRVROS_INTEL_BLITTER_TRIAL
 if(e==4 && w==2)*v=bcs_entry_disabled?0:0x3eef000;
 if(e==4 && w==3)*v=bcs_ctl;
#endif
 if(!strcmp(engine_mode,"active") && e==0 && w==3)*v=1;
 if(!strcmp(engine_mode,"pending") && e==2 && w==4)*v=0x10;
 if(!strcmp(engine_mode,"changed") && engine_calls>30 && e==1 && w==2)*v=4096;
 if((!strcmp(engine_mode,"float") && e==0 && w==0) || (!strcmp(engine_mode,"float-last") && engine_calls==60))*v=UINT32_MAX;
#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
 if(engine_calls>60 && !strcmp(engine_mode,"blit-post-change") && e==0 && w==2)*v=4096;
 if(engine_calls>60 && !strcmp(engine_mode,"blit-post-enabled") && e==4 && w==3)*v|=1;
 if(engine_calls>60 && !strcmp(engine_mode,"blit-post-pending") && e==0 && w==4)*v|=0x10;
 if(engine_calls>60 && !strcmp(engine_mode,"blit-post-float"))*v=UINT32_MAX;
 if(engine_calls>60 && !strcmp(engine_mode,"blit-post-foreign"))wake_request|=2;
#endif
 return strcmp(engine_mode,"read-fail")!=0 && (strcmp(engine_mode,"read-fail-last")!=0 || engine_calls!=60);
}
#endif
#ifdef SRVROS_INTEL_COMPLETION_TRIAL
static bool intel_display_test_completion_read(unsigned e,unsigned i,uint32_t reg,uint32_t *out) {
 assert(e<5 && i<8 && wake_irq && wake_ack==1 && !display_access_allowed());
 static const uint32_t base[5]={0x2000,0x12000,0x1a000,0x1c000,0x22000},tlb[5]={0x4260,0x4264,0x4270,0x4268,0x426c},off[7]={0x30,0x34,0x38,0x3c,0x234,0x238,0x2d0};
 assert(reg==(i==7?tlb[e]:base[e]+off[i]));
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
 census_fault(e,i);
#endif
#ifdef SRVROS_INTEL_BCS_RESET_TRIAL
 if(bcs_reset_writes && !strcmp(engine_mode,"bcs-postread"))return false;
#endif
 if(i<6)return intel_display_test_engine_read(e,i,out);
 *out=0;
 if(i==6 && !strcmp(engine_mode,"flip-wait"))*out=1u<<29;
 if(i==7 && !strcmp(engine_mode,"tlb-pending"))*out=1;
 if(i==7 && !strcmp(engine_mode,"tlb-reserved"))*out=2;
 if(i==7 && !strcmp(engine_mode,"tlb-float"))*out=UINT32_MAX;
 return strcmp(engine_mode,"tlb-read")!=0;
}
static bool intel_display_test_completion_context(unsigned reg,uint32_t *out) {
 unsigned i=0;static const uint32_t regs[6]={0x209c,0x20c0,0x229c,0x2209c,0x220c0,0x2229c};
 for(;i<6 && regs[i]!=reg;i++);
 assert(i<6 && wake_irq && wake_ack==1 && !display_access_allowed());completion_context_reads++;
 *out=(i==0 || i==3)?0x200:i==1?0x4080:0;
#ifdef SRVROS_INTEL_BCS_RESET_TRIAL
 if(bcs_reset_writes && !strcmp(engine_mode,"bcs-postengine") && i==1)*out^=1;
#endif
 if(!strcmp(engine_mode,"context-change") && completion_context_reads>6)*out^=1;
 if(!strcmp(engine_mode,"context-float"))*out=UINT32_MAX;
 return strcmp(engine_mode,"context-read")!=0;
}
#endif
static bool fw_mode,fw_bad_signature,fw_changed,fw_cache_fail,fw_map_fail,fw_retire_fail;
static unsigned fw_reads;
static bool intel_display_test_fw_wb(uint64_t);
static bool intel_display_test_fw_read(uint32_t,uint32_t *);
static bool gt_mode,gt_cache_fail,gt_map_fail,gt_retire_fail,gt_changed,gt_bad_pci,gt_float;
static unsigned gt_word_count;
static bool intel_display_test_gt_uc(uint64_t,uint32_t);
static bool handoff_drift;
static bool cache_mode,window_mode,window_map_fail,window_retire_fail,window_cache_fail;
static bool intel_display_test_uc(uint64_t);
static bool intel_display_test_read(uint32_t,uint32_t *);
#ifdef SRVROS_INTEL_TAKEOVER_PREFLIGHT
static bool intel_display_test_wake_snapshot(uint32_t *r,uint32_t *a,uint32_t *t) {
#ifdef SRVROS_INTEL_JOINT
 intel_display_test_joint_snapshot_delay();
#endif
 return intel_display_test_wake_read(0xa188,r) && intel_display_test_wake_read(0x130044,a) && intel_display_test_wake_read(0x13805c,t);
}
static unsigned takeover_reads;
static bool intel_display_test_takeover_read(unsigned,uint32_t,uint32_t *);
#endif
#define SRVROS_INTEL_DISPLAY_HOST_TEST
#include "../kernel/src/drivers/intel_display_probe.c"

#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
static struct intel_gt_access *shutdown_saved_owner;
static struct intel_gt_borrow *shutdown_saved_borrow;
static void *shutdown_saved_context;
void intel_display_test_shutdown_lifetime(struct intel_gt_access *g,struct intel_gt_borrow *b,void *context) {
 shutdown_saved_owner=g;shutdown_saved_borrow=b;shutdown_saved_context=context;
}
__attribute__((noinline)) static void shutdown_clobber_stack(void) {
 volatile unsigned char bytes[32768];
 for(unsigned i=0;i<sizeof bytes;i++)bytes[i]=(unsigned char)i;
 __asm__ volatile(""::"r"(&bytes[0]):"memory");
}
__attribute__((noinline)) static void shutdown_check_lifetime(void) {
 shutdown_clobber_stack();
 assert(shutdown_saved_owner && shutdown_saved_borrow && shutdown_saved_context);
 struct intel_gt_access *g=shutdown_saved_owner;
 struct intel_gt_borrow *b=shutdown_saved_borrow;
 if(g->object)assert(g->object==g && g->io.context==shutdown_saved_context);
 if(b->object)assert(b->object==b && (b->parent==g || (!b->parent && !b->active)));
 if(b->active)assert(g->pinned && g->dependents==b && g->borrows==1);
 /* Reading the retained provider after return also exercises ASan's lifetime
  * instrumentation. Do not invoke I/O through an uncertain owner. */
 struct wake_adapter *a=shutdown_saved_context;
#ifdef SRVROS_INTEL_HDA_SHUTDOWN
 assert((!joint.wake || a==joint.wake) && a->maps<=WAKE_MAPS && a->retired<=a->maps);
 if(intel_gt_wake_retained())assert(!display_access_allowed());
#else
 if(intel_gt_wake_retained())assert(g->pinned || a->uncertain || a->fw.uncertain || trial_mapping_fault);
#endif
}
#endif

#ifdef SRVROS_INTEL_WAKE_TRIAL
static enum intel_gt_write intel_display_test_wake_write(uint32_t v){
#ifdef SRVROS_INTEL_JOINT
 assert(intel_display_test_joint_inhibited());
#endif
 assert(wake_irq && !display_access_allowed());wake_writes++;
 assert((wake_writes==1 && v==0xffff0000u) || (wake_writes==2 && v==0x10001) || (wake_writes==3 && v==0x10000));
 wake_request=(v&0xffff0000u)|((wake_request&0xffffu)&~(v>>16))|(v&0xffffu);
 if(wake_late_foreign)wake_request|=2;
 if(!wake_no_ack)wake_ack=wake_request&0xffffu;
 return wake_uncertain?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
static struct limine_firmware_type_response wake_firmware={.firmware_type=2};
static struct limine_bootloader_info_response wake_loader={.name="Limine",.version="11.4.1"};
static struct limine_executable_cmdline_response wake_cmd={.cmdline="srvros.storage=ram srvros.workspace-volume=37d8c5d0 srvros.desktop-release=0123456789abcdef srvros.capture=intel-display srvros.intel-backing=read-only srvros.intel-cache=read-only srvros.intel-window=read-only srvros.intel-handoff=qualify srvros.intel-gt-preflight=read-only srvros.intel-firmware=read-only srvros.intel-wake=trial srvros.capture-failure=reboot"};
#ifdef SRVROS_INTEL_GPU_HOLD
#define intel_wake_firmware_request intel_gpu_hold_firmware_request
#elif defined(SRVROS_INTEL_HDA_SHUTDOWN)
#define intel_wake_firmware_request intel_hda_shutdown_firmware_request
#elif defined(SRVROS_INTEL_JOINT)
#define intel_wake_firmware_request intel_joint_firmware_request
#elif defined(SRVROS_INTEL_REHOLD)
#define intel_wake_firmware_request intel_rehold_firmware_request
#elif defined(SRVROS_INTEL_HELD_INTERVAL)
#define intel_wake_firmware_request intel_held_interval_firmware_request
#elif defined(SRVROS_INTEL_PREPARE_RESET)
#define intel_wake_firmware_request intel_held_reset_firmware_request
#elif defined(SRVROS_INTEL_HELD_REQUESTER)
#define intel_wake_firmware_request intel_held_requester_firmware_request
#elif defined(SRVROS_INTEL_REQUESTER_AUDIT)
#define intel_wake_firmware_request intel_requester_firmware_request
#elif defined(SRVROS_INTEL_ENGINE_PREPARE)
#define intel_wake_firmware_request intel_engine_prepare_firmware_request
#elif defined(SRVROS_INTEL_TAKEOVER_PREFLIGHT)
#define intel_wake_firmware_request intel_takeover_firmware_request
#elif defined(SRVROS_INTEL_SHUTDOWN_TRIAL)
#define intel_wake_firmware_request intel_shutdown_firmware_request
#elif defined(SRVROS_INTEL_BCS_SHUTDOWN_TRIAL)
#define intel_wake_firmware_request intel_bcs_shutdown_firmware_request
#elif defined(SRVROS_INTEL_BCS_RESET_TRIAL)
#define intel_wake_firmware_request intel_bcs_reset_firmware_request
#elif defined(SRVROS_INTEL_RESET_READY_TRIAL)
#define intel_wake_firmware_request intel_reset_ready_firmware_request
#elif defined(SRVROS_INTEL_COMPLETION_TRIAL)
#define intel_wake_firmware_request intel_completion_firmware_request
#endif
volatile struct limine_firmware_type_request intel_wake_firmware_request={.response=&wake_firmware};
volatile struct limine_bootloader_info_request bootloader_info_request={.response=&wake_loader};
volatile struct limine_executable_cmdline_request executable_cmdline_request={.response=&wake_cmd};
#endif

#ifdef SRVROS_INTEL_BLITTER_TRIAL
static bool intel_display_test_blitter_read(uint32_t *v) {
 assert(wake_irq && !display_access_allowed() && wake_ack==1);bcs_reads++;
 const uint32_t words[8]={0,0,bcs_entry_disabled?0:0x3eef000,bcs_ctl,1,0,0x200,1};
 for(unsigned i=0;i<8;i++)v[i]=words[i];
#ifdef SRVROS_INTEL_HDA_SHUTDOWN
 if((!strcmp(engine_mode,"paired-entry-1") && bcs_reads==1) || (!strcmp(engine_mode,"paired-entry-2") && bcs_reads==2))v[2]^=4096;
#endif
 if(!strcmp(engine_mode,"blit-notidle"))v[6]=0;
 if(!strcmp(engine_mode,"blit-execlist"))v[7]|=0x8000;
 if(!strcmp(engine_mode,"blit-ppgtt"))v[7]|=0x200;
 if(!strcmp(engine_mode,"blit-mode-change") && bcs_writes)v[7]=0;
 if(!strcmp(engine_mode,"blit-busy"))v[0]=8;
 if(!strcmp(engine_mode,"blit-start"))v[2]=0x3eee000;
 if(!strcmp(engine_mode,"blit-change") && bcs_reads==2)v[6]|=1u<<16;
 if(!strcmp(engine_mode,"blit-float"))v[0]=UINT32_MAX;
 if(!strcmp(engine_mode,"blit-late-foreign") && bcs_writes)wake_request|=2;
 if(!strcmp(engine_mode,"blit-off-busy"))v[0]=8;
 if(!strcmp(engine_mode,"blit-off-geometry"))v[2]=0x3eef000;
 if(!strcmp(engine_mode,"blit-off-enabled"))v[3]=0xf001;
 if(!strcmp(engine_mode,"blit-off-notidle"))v[6]=0;
 if(!strcmp(engine_mode,"blit-off-execlist"))v[7]|=0x8000;
 if(!strcmp(engine_mode,"blit-off-drift") && bcs_reads==3)v[6]|=1u<<16;
 if(!strcmp(engine_mode,"blit-off-float"))v[0]=UINT32_MAX;
 return strcmp(engine_mode,"blit-read-fail")!=0;
}
static enum intel_gt_write intel_display_test_blitter_write(uint32_t value) {
 assert(!bcs_entry_disabled && wake_irq && !display_access_allowed() && wake_ack==1 && value==0xf000 && bcs_ctl==0xf001);
 for(unsigned i=0;i<WAKE_MAPS;i++){uint64_t physical;assert(vmm_virt_to_phys_in_space(0x1000,WAKE_VA+4096*i,&physical) && physical==0xc0000000ull+wake_pages[i]);}
 bcs_writes++;assert(bcs_writes==1);
 if(!strcmp(engine_mode,"blit-unissued"))return GT_WRITE_NONE;
 if(strcmp(engine_mode,"blit-readback"))bcs_ctl=value;
 return !strcmp(engine_mode,"blit-uncertain")?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
static bool intel_display_test_reader_read(unsigned p,unsigned w,uint32_t *out) {
 assert(p<3 && w<9 && wake_irq && !display_access_allowed() && wake_ack==1);reader_calls++;
 *out=!p && !w?0xd8000400:0;
#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
 if(!strcmp(engine_mode,"blit-reader-primary") && !p && !w)*out=0x98000400;
#endif
 if((!strcmp(engine_mode,"blit-reader-change") || !strcmp(engine_mode,"blit-off-reader-change")) && reader_calls>27 && !p && w==1)*out=4096;
 return strcmp(engine_mode,"blit-reader-fail")!=0;
}
#endif
bool console_sync(void){return true;}
void console_flush(void){assert(display_access_allowed());}
bool display_present_handoff_idle(void){return true;}
static bool intel_display_test_gt_uc(uint64_t p,uint32_t offset){assert((offset==0xa000 || offset==0x130000) && p==0xc0000000ull+offset);return !gt_cache_fail;}
static bool intel_display_test_fw_wb(uint64_t physical){assert(physical==0x12345000);return !fw_cache_fail;}
static bool intel_display_test_fw_read(uint32_t offset,uint32_t *v){
 assert(!display_access_allowed());fw_reads++;
 const uint32_t values[]={0x65746e49,0x6172476c,0x63696870,0x6d654d73,8,0x02000000,wake_mode?31:7,0,0,0,0,0,0};
 for(unsigned i=0;i<INTEL_FW_WORDS;i++)if(offset==intel_fw_offsets[i]){*v=values[i];if(fw_bad_signature && !i)*v=0;if(fw_changed && fw_reads>13 && i==7)*v=1;if((wake_busy || (wake_fw_after && wake_writes)) && i==7)*v=1;return true;}
 assert(0);return false;
}
static unsigned config_reads,mmio_reads,map_calls,unmap_calls;
static uint64_t ticks,current,pa[INTEL_CAPTURE_PAGES+16];
#ifdef SRVROS_INTEL_TAKEOVER_PREFLIGHT
static bool intel_display_test_takeover_read(unsigned i,uint32_t reg,uint32_t *out) {
 static const uint32_t offsets[]={0x20d0,0x120d0,0x1a0d0,0x1c0d0,0x220d0,0x941c};
 assert(i<6 && reg==offsets[i] && wake_irq && !display_access_allowed() && wake_ack==1);
 ++takeover_reads;*out=0;
#ifdef SRVROS_INTEL_ENGINE_PREPARE
 if(i<5)*out=prepare_controls[i];
#endif
 if(!strcmp(engine_mode,"take-read") && takeover_reads==3)return false;
 if(!strcmp(engine_mode,"take-float") && takeover_reads==5)*out=UINT32_MAX;
 if(!strcmp(engine_mode,"take-change") && takeover_reads>6 && i==2)*out=1;
 if(!strcmp(engine_mode,"take-busy") && i==0)*out=3;
 if(!strcmp(engine_mode,"take-mask") && i<5)*out=0xffff0000;
 if(!strcmp(engine_mode,"take-reserved") && i==1)*out=4;
 if(!strcmp(engine_mode,"take-domain") && i==5)*out=8;
 if(!strcmp(engine_mode,"take-domain-reserved") && i==5)*out=32;
 if(!strcmp(engine_mode,"take-foreign") && takeover_reads==3)wake_request|=2;
 if(!strcmp(engine_mode,"take-map") && takeover_reads==2)pa[INTEL_CAPTURE_PAGES+1+9]^=4096;
 return true;
}
#ifdef SRVROS_INTEL_ENGINE_PREPARE
static enum intel_gt_write intel_display_test_prepare_transition(unsigned e,uint32_t value,uint32_t *out) {
#ifdef SRVROS_INTEL_JOINT
 assert(intel_display_test_joint_inhibited());
#endif
 unsigned n=prepare_stores++;assert(n<10 && e==(n<5?n:9-n));
 assert(wake_irq && wake_ack==1 && !display_access_allowed() && value==(n<5?0x10001u:0x10000u));
 unsigned point=1;const char *colon=strchr(engine_mode,':');if(colon)point=(unsigned)atoi(colon+1);
 if(!strncmp(engine_mode,"prep-none",9) && prepare_stores==point)return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_PREPARE_RESET
 if(n>=5) {
  assert(held_boundary==2 && held_reads==18 && held_reset_boundary==2 && held_reset_writes==2);
  assert(!prepare_controls[4]);
 }
#endif
 prepare_controls[e]=n<5?3:0;*out=prepare_controls[e];
 if(!strncmp(engine_mode,"prep-uncertain",14) && prepare_stores==point)return GT_WRITE_UNCERTAIN;
 if(!strncmp(engine_mode,"prep-foreign",12) && prepare_stores==point)wake_request|=2;
 if(!strncmp(engine_mode,"prep-ack-float",14) && prepare_stores==point)*out=UINT32_MAX;
 if(!strncmp(engine_mode,"prep-map",8) && prepare_stores==point)pa[INTEL_CAPTURE_PAGES+1+3+e]^=4096;
 return GT_WRITE_DONE;
}
static bool intel_display_test_prepare_reader(uint32_t reg,uint32_t *out) {
 static const uint32_t offsets[9]={0x180,0x19c,0x1ac,0x80,0x84,0xac,0x280,0x29c,0x2ac};
 unsigned p=(reg-0x70000)/4096,i=prepare_reader_calls++%9;
 assert(p<3 && reg==0x70000+p*4096+offsets[i] && wake_irq && wake_ack==1 && !display_access_allowed());
 *out=(!p && !i)?0xd8000400u:0;
 if(prepare_stores==10 && !strcmp(engine_mode,"prep-plane") && i==8)*out=1;
 return !(prepare_stores==10 && !strcmp(engine_mode,"prep-plane-read"));
}
#endif
#endif
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
static void census_fault(unsigned e,unsigned i) {
 bool post=bcs_reset_writes!=0;
 if(e==0 && i==7) {
  if(!strcmp(engine_mode,"census-foreign-pre") || (post && !strcmp(engine_mode,"census-foreign-post")))wake_request|=2;
  if(!strcmp(engine_mode,"census-map-pre") || (post && !strcmp(engine_mode,"census-map-post")))pa[INTEL_CAPTURE_PAGES+1+4]+=4096;
 }
 if(e==1 && i==0) {
  if(!strcmp(engine_mode,"census-foreign-pre") || (post && !strcmp(engine_mode,"census-foreign-post")))assert((wake_request&65535)==1);
  if(!strcmp(engine_mode,"census-map-pre") || (post && !strcmp(engine_mode,"census-map-post")))assert(pa[INTEL_CAPTURE_PAGES+1+4]==0xc0012000ull);
 }
}
#endif

static bool firmware_ok,clock_ok,alias,malformed,reporting,backing_mode,translation_gap,dma_unmap_fail;
static struct pci_device devices[4];
static struct acpi_table_info dsdt,extra_dmar;
static bool extra_table;
static uint8_t extra_bytes[136];
static struct limine_memmap_entry ram,aperture,stolen_region,firmware_region;
static struct limine_memmap_entry *entries[]={&ram,&aperture,&stolen_region,&firmware_region};
static struct limine_memmap_response memory;
volatile struct limine_memmap_request memmap_request;
static struct limine_framebuffer frame;
static const uint64_t fb_virtual=0xffff8000b0000000ull;
static bool intel_display_test_uc(uint64_t p){assert(p==0xc0811000);return !window_cache_fail;}
uint8_t pci_read_config8(const struct pci_device *d,uint8_t off){assert(d==&devices[0] && off==0x62);return 1;}
uint64_t pci_device_count(void){
#if defined(SRVROS_INTEL_BCS_RESET_TRIAL) || defined(SRVROS_INTEL_BLITTER_TRIAL) || defined(SRVROS_INTEL_HDA_OBSERVER) || defined(SRVROS_INTEL_HDA_OWNER) || (defined(SRVROS_INTEL_JOINT) || defined(SRVROS_INTEL_HDA_SHUTDOWN)) || defined(SRVROS_INTEL_ENGINE_PREPARE)
 return 4;
#else
 return 3;
#endif
}
const struct pci_device *pci_device_at(uint64_t i){return i<pci_device_count()?&devices[i]:NULL;}
uint32_t pci_read_config32(const struct pci_device *d,uint8_t off) {
#if defined(SRVROS_INTEL_HDA_OBSERVER) || defined(SRVROS_INTEL_HDA_OWNER) || (defined(SRVROS_INTEL_JOINT) || defined(SRVROS_INTEL_HDA_SHUTDOWN))
    if(d==&devices[3])return intel_display_test_hda_config(off);
#endif
#ifdef SRVROS_INTEL_ENGINE_PREPARE
    if(d==&devices[3]){switch(off){case 0:return 0x160c8086;case 4:return 0x00100006;case 8:return 0x04030001;case 0x34:return 0x40;case 0x40:return 0x00030001;case 0x44:return prepare_stores==10 && !strcmp(engine_mode,"prep-audio")?0:3;default:return 0;}}
#endif
#ifdef SRVROS_INTEL_GPU_HOLD
    if(d==&devices[0] && off==4)return 0x00100000|intel_display_test_gpu_hold_command();
#endif
    config_reads++;assert(config_reads<(wake_mode?10000000:gt_mode?20000:window_mode?5000:1000));
#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
    if(d==&devices[3] && (!strcmp(engine_mode,"blit-link-change") || !strcmp(engine_mode,"blit-ltr-change"))) {
     if(off==0x40)return 0x00035001;
     if(off==0x50)return 0x00020010;
     if(off==0x5c)return reader_calls>27 && !strcmp(engine_mode,"blit-link-change")?2:1;
     if(off==0x74)return reader_calls>27 && !strcmp(engine_mode,"blit-ltr-change")?2:1;
    }
#endif
#ifdef SRVROS_INTEL_BCS_RESET_TRIAL
    if(d==&devices[3]){switch(off){case 0:return 0x160c8086;case 4:return 0x00100006;case 8:return 0x04030001;case 0x34:return 0x40;case 0x40:return 0x00030001;case 0x44:return bcs_reset_writes && !strcmp(engine_mode,"bcs-audio")?0:3;default:return 0;}}
#endif
#ifdef SRVROS_INTEL_BLITTER_TRIAL
    if(d==&devices[3]){switch(off){case 0:return 0x160c8086;case 4:return 0x00100006;case 8:return 0x04030001;case 0x34:return 0x40;case 0x40:return 0x00030001;case 0x44:return reader_calls>27 && (!strcmp(engine_mode,"blit-audio-change") || !strcmp(engine_mode,"blit-off-audio-change"))?0:3;default:return 0;}}
#endif
    if(d==&devices[2]){switch(off){case 0:return 0x16048086;case 0x50:return window_mode?0x2c0:0x140;case 0xb0:return window_mode?0x8c000001:0x90000001;case 0xb4:return window_mode?0x8b800001:0x8fe00001;case 0xbc:return window_mode?0x90000001:0x92000001;default:return 0;}}
    if(d==&devices[1])return off==0?0x9cc38086:0;
    switch(off) {
    case 0:return 0x16268086;case 4:return 0x00100006;case 8:return 0x03000009;
    case 0xfc:
#ifdef SRVROS_INTEL_PREPARE_RESET
        if((held_reset_boundary==1 && !held_reset_reads && !strcmp(engine_mode,"hreset-entry-endpoint")) || (held_reset_boundary==2 && !strcmp(engine_mode,"hreset-exit-endpoint")))return 1;
#endif
#ifdef SRVROS_INTEL_HELD_REQUESTER
        if((held_boundary==1 && !held_reads && !strcmp(engine_mode,"held-entry-endpoint")) || (held_boundary==2 && !strcmp(engine_mode,"held-exit-endpoint")))return 1;
#endif
#ifdef SRVROS_INTEL_REQUESTER_AUDIT
        if(!strncmp(engine_mode,"req-exit-endpoint:",18) && requester_reads==9u*(unsigned)atoi(engine_mode+18))return 1;
#endif
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
        if(bcs_exit_fault("bcs-exit-endpoint") || bcs_post_exit_fault("bcs-post-exit-endpoint"))return 1;
#endif
#ifdef SRVROS_INTEL_ENGINE_PREPARE
        if(prepare_exit_fault("prep-exit-endpoint") || prepare_post_exit_fault("prep-post-exit-endpoint"))return 1;
#endif
        return gt_bad_pci?UINT32_MAX:gt_mode?0x12345000:0;case 0xe8:return gt_mode?0x8000:0;
    case 0x10:return 0xc0000004;case 0x18:return 0xb000000c;case 0x2c:return 0x011b106b;
    case 0x50:return window_mode?0x2c0:0x140;case 0x5c:return window_mode?0x8c000001:0x90000001;
#if defined(SRVROS_INTEL_REQUESTER_AUDIT) || defined(SRVROS_INTEL_HELD_REQUESTER)
    case 0x34:return 0x90;case 0x90:return 0x0000d005;case 0xd0:return 0x0002a401;
    case 0xa4:return 0x03060013;
#ifdef SRVROS_INTEL_REQUESTER_AUDIT
    case 0xa8:return !strcmp(engine_mode,"req-pending") || (!strcmp(engine_mode,"req-pending-transition") && requester_reads>9)?256:0;
#else
    case 0xa8:return 0; /* Held sampler injects its own status at issue. */
#endif
#else
    case 0x34:return 0x40;
#endif
    case 0x40:return malformed?0x00034001:0x00030001;
    default:return 0;
    }
}
uint64_t acpi_table_count(void){return extra_table?2:1;}
const struct acpi_table_info *acpi_table_at(uint64_t i){return i==0?&dsdt:i==1 && extra_table?&extra_dmar:NULL;}
void *pmm_phys_to_virt(uint64_t p){return (void *)(uintptr_t)p;}
bool laptop_firmware_match(const uint8_t *p,size_t n){assert((uintptr_t)p==0x100000 && n==35034);return firmware_ok;}
uint64_t vmm_kernel_address_space(void){return 0x1000;}
uint64_t vmm_current_address_space(void){return current;}
bool vmm_virt_to_phys(uint64_t v,uint64_t *p){assert(v==fb_virtual);*p=0xb0000000;return true;}
static unsigned index_of(uint64_t v){
#ifdef SRVROS_INTEL_HELD_INTERVAL
 if(v==BACKING_VA+0x1d000)return INTEL_CAPTURE_PAGES+14;
#endif
#ifdef SRVROS_INTEL_READER_TRIAL
 if(v==READER_VA)return INTEL_CAPTURE_PAGES+13;
#endif
#if defined(SRVROS_INTEL_HDA_OBSERVER) || defined(SRVROS_INTEL_HDA_OWNER) || (defined(SRVROS_INTEL_JOINT) || defined(SRVROS_INTEL_HDA_SHUTDOWN))
#ifdef SRVROS_INTEL_JOINT
 if(v==HDA_VA)return INTEL_CAPTURE_PAGES+15;
#else
 if(v==HDA_VA)return INTEL_CAPTURE_PAGES+12;
#endif
#endif
 if(v>=BACKING_VA+0x10000 && v<BACKING_VA+0x1d000)return INTEL_CAPTURE_PAGES+1+(unsigned)((v-BACKING_VA-0x10000)/4096);
 if(v==BACKING_VA)return INTEL_CAPTURE_PAGES;
 assert(v>=CAPTURE_VA && v<CAPTURE_VA+4096*INTEL_CAPTURE_PAGES && !(v&4095));
 return (unsigned)((v-CAPTURE_VA)/4096);
}
bool vmm_virt_to_phys_in_space(uint64_t space,uint64_t v,uint64_t *p){assert(space==0x1000);if(backing_mode && v>=fb_virtual && v<fb_virtual+6144*900){if(translation_gap && v==fb_virtual+4096)return false;*p=0xb0000000+v-fb_virtual;return true;}unsigned n=index_of(v);*p=pa[n];return pa[n]!=0;}
bool vmm_diagnostic_contiguous_kernel_range(uint64_t space,uint64_t va,uint64_t physical,uint64_t pages) {
 assert(space==0x1000 && wake_irq && !(va&4095) && !(physical&4095) && pages && pages<=4096);
#ifdef SRVROS_INTEL_PREPARE_RESET
 if(++range_calls==range_fail_at)return false;
#endif
 if(current!=space)return false;
 for(uint64_t i=0;i<pages;i++){uint64_t p;if(!vmm_virt_to_phys_in_space(space,va+4096*i,&p) || p!=physical+4096*i)return false;}
 return true;
}
bool vmm_diagnostic_contiguous_kernel_range_observe(uint64_t space,uint64_t va,uint64_t physical,uint64_t pages,struct vmm_range_observation *d) {
 bool ok=vmm_diagnostic_contiguous_kernel_range(space,va,physical,pages);
 if(d)*d=ok?(struct vmm_range_observation){.phase=4,.checked=pages}:
  (struct vmm_range_observation){.phase=1,.reason=VMM_RANGE_COUNT,.detail=3};
 return ok;
}
bool vmm_map_page_in_space(uint64_t space,uint64_t v,uint64_t p,uint64_t flags) {
    bool writable=wake_mode && v==BACKING_VA+0x10000;
#ifdef SRVROS_INTEL_ENGINE_PREPARE
    writable=writable || (wake_mode && v>=BACKING_VA+0x13000 && v<=BACKING_VA+0x17000);
#endif
#if defined(SRVROS_INTEL_BLITTER_TRIAL) || defined(SRVROS_INTEL_RESET_READY_TRIAL) || defined(SRVROS_INTEL_BCS_RESET_TRIAL)
    writable=writable || (wake_mode && v==BACKING_VA+0x17000);
#endif
#ifdef SRVROS_INTEL_BCS_RESET_TRIAL
    writable=writable || (wake_mode && v==BACKING_VA+0x19000);
    if(!strcmp(engine_mode,"bcs-map") && v==BACKING_VA+0x19000)return false;
#endif
#ifdef SRVROS_INTEL_PREPARE_RESET
    writable=writable || (wake_mode && v==BACKING_VA+0x19000);
    if(!strcmp(engine_mode,"hreset-map-slot") && v==BACKING_VA+0x19000)return false;
#endif
#ifdef SRVROS_INTEL_HELD_INTERVAL
    if(!strcmp(engine_mode,"interval-map-slot") && v==BACKING_VA+0x1d000)return false;
    if(v==BACKING_VA+0x1d000)assert(!writable && flags==(VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE));
#endif
    assert(space==0x1000 && flags==(writable?(VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE|VMM_PAGE_WRITABLE):(fw_mode && p==0x12345000)?VMM_PAGE_NO_EXECUTE:(VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE)));unsigned n=index_of(v);
    assert(!pa[n]);if(n>=INTEL_CAPTURE_PAGES)assert((p>=0xc0000000 && p<0xc1000000)
#if defined(SRVROS_INTEL_HDA_OBSERVER) || defined(SRVROS_INTEL_HDA_OWNER) || (defined(SRVROS_INTEL_JOINT) || defined(SRVROS_INTEL_HDA_SHUTDOWN))
 || (p==INTEL_HDA_BASE && v==HDA_VA)
#endif
 || p==0xfed90000 || (fw_mode && p==0x12345000));else assert(p==0xc0000000ull+intel_capture_pages[n]);pa[n]=p;map_calls++;
#ifdef SRVROS_INTEL_READER_TRIAL
 if(v==READER_VA && !intel_display_test_pinned_map())return false;
#endif

#ifdef SRVROS_INTEL_ENGINE_TRIAL
 if(!strcmp(engine_mode,"map-last") && v==BACKING_VA+0x17000)return false;
#ifdef SRVROS_INTEL_COMPLETION_TRIAL
 if(!strcmp(engine_mode,"tlb-map") && v==BACKING_VA+0x18000)return false;
#endif
#endif
#if defined(SRVROS_INTEL_HDA_OBSERVER) || defined(SRVROS_INTEL_HDA_OWNER) || (defined(SRVROS_INTEL_JOINT) || defined(SRVROS_INTEL_HDA_SHUTDOWN))
 if(v==HDA_VA && !intel_display_test_hda_map())return false;
#endif
 if((window_map_fail && p==0xc0811000) || (gt_map_fail && p==0xc000a000) || (fw_map_fail && p==0x12345000) || (wake_map_fail && v==BACKING_VA+0x10000))return false;
 return true;
}
bool vmm_unmap_page_in_space(uint64_t space,uint64_t v,uint64_t *p){
#ifdef SRVROS_INTEL_HELD_INTERVAL
 if(!strcmp(engine_mode,"interval-retire-slot") && v==BACKING_VA+0x1d000)return false;
#endif
#ifdef SRVROS_INTEL_READER_TRIAL
 if(v==READER_VA && !intel_display_test_pinned_retire())return false;
#endif
#if defined(SRVROS_INTEL_HDA_OBSERVER) || defined(SRVROS_INTEL_HDA_OWNER) || (defined(SRVROS_INTEL_JOINT) || defined(SRVROS_INTEL_HDA_SHUTDOWN))
 if(v==HDA_VA && !intel_display_test_hda_retire())return false;
#endif
assert(space==0x1000);unsigned n=index_of(v);assert(pa[n]);
#ifdef SRVROS_INTEL_ENGINE_TRIAL
 if(!strcmp(engine_mode,"retire-last") && v==BACKING_VA+0x17000)return false;
#ifdef SRVROS_INTEL_COMPLETION_TRIAL
 if(!strcmp(engine_mode,"tlb-retire") && v==BACKING_VA+0x18000)return false;
#endif
#endif
#ifdef SRVROS_INTEL_BCS_RESET_TRIAL
 if(!strcmp(engine_mode,"bcs-retire") && v==BACKING_VA+0x19000)return false;
#endif
#ifdef SRVROS_INTEL_PREPARE_RESET
 if(!strcmp(engine_mode,"hreset-retire-slot") && v==BACKING_VA+0x19000)return false;
#endif
 if((wake_retire_fail && v==BACKING_VA+0x10000) || (fw_retire_fail && pa[n]==0x12345000) || dma_unmap_fail || (window_retire_fail && pa[n]==0xc0811000) || (gt_retire_fail && pa[n]==0xc000a000))return false;
 *p=pa[n];pa[n]=0;unmap_calls++;return true;}
bool timer_reference_ready(void){return clock_ok;}
uint64_t timer_monotonic_us(void){
#ifdef SRVROS_INTEL_HDA_SHUTDOWN
 if(paired_clock_fault){unsigned f=paired_clock_fault;paired_clock_fault=0;return f==1?0:1;}
#endif
#ifdef SRVROS_INTEL_PREPARE_RESET
 if(held_reset_clock_fault) {
  unsigned fault=held_reset_clock_fault;held_reset_clock_fault=0;
  if(fault==1)return 0;
  if(fault==2)return 1;
 }
#endif
#ifdef SRVROS_INTEL_ENGINE_TRIAL
#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
 if(engine_calls>60 && !strcmp(engine_mode,"blit-post-clock"))ticks+=200000;
 if(engine_calls>60 && !strcmp(engine_mode,"blit-post-backward"))return 1;
#endif
 if(engine_calls && !strcmp(engine_mode,"slow"))ticks+=200000;
 if(engine_calls && !strcmp(engine_mode,"frozen"))return ticks;
#endif
 ticks+=wake_irq?1:100;return ticks;}
void console_write(const char *s){if(reporting)fputs(s,stdout);}
/* Kernel %u/%x arguments are uint64_t; adapt those conversions for host libc. */
void console_printf(const char *format,...) {
    if(!reporting)return;
    char translated[2048];size_t n=0;
    for(size_t i=0;format[i];i++) {
        if(format[i]=='%' && (format[i+1]=='u' || format[i+1]=='x')) {
            translated[n++]='%';
            if(format[i+1]=='x')translated[n++]='#';
            const char *suffix=format[i+1]=='u'?PRIu64:PRIx64;
            while(*suffix)translated[n++]=*suffix++;
            i++;
        } else translated[n++]=format[i];
    }
    translated[n]=0;va_list ap;va_start(ap,format);vprintf(translated,ap);va_end(ap);
}
static bool intel_display_test_read(uint32_t off,uint32_t *value) {
#ifdef SRVROS_INTEL_ENGINE_PREPARE
    if(backing_batch_test_fault && off>=0x800000 && off<0x803000) {
        ++backing_batch_test_reads;
        if(backing_batch_test_reads==backing_batch_test_at) {
            if(backing_batch_test_fault==1)return false;
            if(backing_batch_test_fault==2)pa[INTEL_CAPTURE_PAGES]^=4096;
            if(backing_batch_test_fault==3)current=0x2000;
            if(backing_batch_test_fault==4)wake_irq=false;
            if(backing_batch_test_fault==5){*value=1;return true;}
            if(backing_batch_test_fault==6)intel_boot_state.client=INTEL_BOOT_LEGACY;
        }
    }
#endif
    if(gt_mode && (off==INTEL_GT_REQUEST || off==INTEL_GT_ACK)) {
        assert(!display_access_allowed());gt_word_count++;*value=gt_float?UINT32_MAX:wake_mode?(off==INTEL_GT_REQUEST?(wake_foreign?0x10002:0x10000):0):2;
        if(gt_changed && gt_word_count==4)*value=3;
        mmio_reads++;return true;
    }
    if(off>=0x1000000 && off<=0x1000024){mmio_reads++;*value=off;return true;}
    if(window_mode && off>=INTEL_WINDOW_PAGE && off<INTEL_WINDOW_PAGE+128) {
        unsigned i=(off-INTEL_WINDOW_PAGE)/8;uint64_t entry=i?0x180000000ull+i*4096+1:0;
        if(handoff_drift && i==15)entry^=4096;
        *value=(uint32_t)(entry>>((off&4)?32:0));mmio_reads++;return true;
    }
    if(backing_mode && (off>=0x100000 || !intel_capture_offset_allowed(off))) {
        *value=0;
        if(off>=0x100000 && off<0x100100){unsigned i=(off-0x100000)/8;uint64_t f=i?0:cache_mode?0x021ff02f00000001ull:((uint64_t)(5554176-4096+47)<<32)|1;*value=(uint32_t)(f>>((off&4)?32:0));}
        else if(off>=0x800000 && off<0x803000){unsigned i=(off-0x800000)/8;*value=(uint32_t)(((window_mode?0x8c001001ull:0x90000001ull)+4096ull*i)>>((off&4)?32:0));}
        else assert(0);
#ifdef SRVROS_INTEL_PREPARE_RESET
        if(((held_reset_boundary==1 && !held_reset_reads && !strcmp(engine_mode,"hreset-entry-backing")) || (held_reset_boundary==2 && !strcmp(engine_mode,"hreset-exit-backing"))) && off>=0x800000 && !(off&7))*value^=4096;
#endif
#ifdef SRVROS_INTEL_HELD_REQUESTER
        if(((held_boundary==1 && !held_reads && !strcmp(engine_mode,"held-entry-backing")) || (held_boundary==2 && !strcmp(engine_mode,"held-exit-backing"))) && off>=0x800000 && !(off&7))*value^=4096;
#endif
#ifdef SRVROS_INTEL_REQUESTER_AUDIT
        if(!strncmp(engine_mode,"req-exit-backing:",17) && requester_reads==9u*(unsigned)atoi(engine_mode+17) && off>=0x800000 && !(off&7))*value^=4096;
#endif
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
        if((bcs_exit_fault("bcs-exit-backing") || bcs_post_exit_fault("bcs-post-exit-backing")) && off>=0x800000 && !(off&7))*value^=4096;
#endif
#ifdef SRVROS_INTEL_ENGINE_PREPARE
        if((prepare_exit_fault("prep-exit-backing") || prepare_post_exit_fault("prep-post-exit-backing")) && off>=0x800000 && !(off&7))*value^=4096;
#endif
        mmio_reads++;return true;
    }
    assert(intel_capture_offset_allowed(off));mmio_reads++;*value=0;
    if(off>=0x45400 && off<=0x4540c)*value=0xc0000000;
    if(off==0x70180)*value=backing_mode?0xd8000400:0x98000000;
    if(off==0x70188)*value=6144;
    if(off==0x6001c)*value=(1439u<<16)|899;
    if(off==0x70000)*value=(uint32_t)ticks%926;
    if(off==0x70040)*value=(uint32_t)(ticks/16667);
    return true;
}
static void reset(void) {
#ifdef SRVROS_INTEL_PREPARE_RESET
    held_reset_boundary=held_reset_reads=held_reset_attempts=held_reset_writes=held_reset_domain_reads=held_reset_clock_fault=0;
#ifdef SRVROS_INTEL_HELD_INTERVAL
    interval_reads=0;
#ifdef SRVROS_INTEL_REHOLD
    rehold_reads=rehold_stores=0;
    rehold_owner_origin=0;
#endif
#endif
    range_calls=range_fail_at=0;
#endif
    memset(&intel_boot_state,0,sizeof(intel_boot_state));
    assert(intel_boot_select(INTEL_BOOT_OBSERVER));
    extra_table=false;
    config_reads=mmio_reads=map_calls=unmap_calls=0;ticks=100000;current=0x1000;
    dma_resource_source=0;dma_resource_declared=false;
    dma_checked=dma_passed=dma_failed=dma_guards=0;dma_memory=(struct dma_memory_snapshot){0};
    dma_region_base=dma_region_length=dma_region_type=0;dma_overlaps=0;dma_attempted=dma_table_valid=dma_preserved=false;dma_result=(struct intel_dma_result){0};
    dma_unmap_fail=false;firmware_ok=clock_ok=true;alias=malformed=reporting=backing_mode=translation_gap=false;backing_attempted=false;trial_mapping_fault=false;backing=(struct intel_backing_result){0};
    memset(pa,0,sizeof(pa));result=(struct intel_capture_result){0};attempted=false;
    devices[0]=(struct pci_device){.device=2,.vendor_id=0x8086,.device_id=0x1626,.class_code=3};devices[1]=(struct pci_device){.device=31,.class_code=6,.subclass=1};
    dsdt=(struct acpi_table_info){.physical_address=0x100000,.length=35034};memcpy(dsdt.signature,"DSDT",4);
    ram=(struct limine_memmap_entry){.base=0x100000,.length=0x70000000,.type=LIMINE_MEMMAP_USABLE};
    devices[2]=(struct pci_device){.device=0,.class_code=6};
#if defined(SRVROS_INTEL_BLITTER_TRIAL) || defined(SRVROS_INTEL_BCS_RESET_TRIAL) || defined(SRVROS_INTEL_ENGINE_PREPARE)
    devices[3]=(struct pci_device){.device=3,.vendor_id=0x8086,.device_id=0x160c,.class_code=4,.subclass=3};
#endif
    aperture=(struct limine_memmap_entry){.base=0xb0000000,.length=0x10000000,.type=LIMINE_MEMMAP_FRAMEBUFFER};
    stolen_region=(struct limine_memmap_entry){.base=0x90000000,.length=0x2000000,.type=LIMINE_MEMMAP_RESERVED};
    memory=(struct limine_memmap_response){.entry_count=1,.entries=entries};memmap_request.response=&memory;
    frame=(struct limine_framebuffer){.address=(void *)(uintptr_t)fb_virtual,.width=1440,.height=900,.pitch=6144,.bpp=32,
        .memory_model=LIMINE_FRAMEBUFFER_RGB,.red_mask_size=8,.red_mask_shift=16,.green_mask_size=8,.green_mask_shift=8,.blue_mask_size=8};
}
int main(int argc,char **argv) {
#ifdef SRVROS_INTEL_PREPARE_RESET
    if(argc==2 && !strcmp(argv[1],"--admission-witness")) {
        reset();backing_mode=true;memory.entry_count=3;
        intel_display_probe_boot(&frame);intel_display_backing_probe_boot(&frame);
        struct gt_observer g={0};assert(trial_adapter(&g.a));
        assert(display_access_acquire(0x41444d49,&g.token));wake_irq=true;admission_begin();
        struct intel_gt_endpoint e={0};range_fail_at=1;
        assert(!gt_guard_endpoint(&g,&e));
        assert(admission_capture.queries==1 && admission_capture.code==7 && admission_capture.pages==1350);
        assert(admission_capture.query.phase==1 && admission_capture.query.reason==VMM_RANGE_COUNT && admission_capture.query.detail==3);
        assert(!admission_fail(302,8) && admission_capture.code==7 && !admission_capture.detail);
        range_fail_at=2;assert(!gt_guard_endpoint(&g,&e));
        assert(admission_capture.queries==2 && admission_capture.query.detail==3);
        admission_active=false;range_fail_at=3;assert(!gt_guard_endpoint(&g,&e));
        assert(admission_capture.queries==2);
        reporting=true;admission_report();assert(display_access_release(&g.token,true));return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--range-endpoint")) {
        reset();backing_mode=true;memory.entry_count=3;
        intel_display_probe_boot(&frame);intel_display_backing_probe_boot(&frame);
        assert(!range_calls); /* IRQ-enabled startup keeps the scalar path. */
        struct gt_observer g={0};assert(trial_adapter(&g.a));
        assert(display_access_acquire(0x52414e47,&g.token));wake_irq=true;
        struct intel_gt_endpoint e={0};
        assert(gt_guard_endpoint(&g,&e) && range_calls==5);
        /* Each endpoint of every new census calls the fresh range query.
         * A refusal never falls back to the scalar walker. */
        for(unsigned n=1;n<=5;n++) {
            range_calls=0;range_fail_at=n;
            assert(!gt_guard_endpoint(&g,&e) && range_calls==n);
        }
        range_fail_at=0;range_calls=0;translation_gap=true;
        assert(!gt_guard_endpoint(&g,&e) && range_calls==1);
        translation_gap=false;range_calls=0;
        assert(gt_guard_endpoint(&g,&e) && range_calls==5);
        assert(display_access_release(&g.token,true));
        puts("range-endpoint: PASS five fresh endpoint comparisons; scalar startup; refusal never falls back");return 0;
    }
#endif
#ifdef SRVROS_INTEL_ENGINE_PREPARE
    if(argc==2 && !strcmp(argv[1],"--guard-endpoint")) {
        reset();backing_mode=true;memory.entry_count=3;
        intel_display_probe_boot(&frame);intel_display_backing_probe_boot(&frame);
        struct gt_observer g={0};assert(trial_adapter(&g.a));
        assert(display_access_acquire(0x47544550,&g.token));wake_irq=true;
        struct intel_gt_endpoint e={0};assert(gt_guard_endpoint(&g,&e) && e.mmio==0xc0000000 && !e.asls && !e.swsci);
        unsigned reads=mmio_reads;
        struct limine_framebuffer *fb=g.a.fb;g.a.fb=&frame;
        assert(!gt_guard_endpoint(&g,&e) && mmio_reads==reads);g.a.fb=fb;
        const struct pci_device *pch=g.a.common.pch;g.a.common.pch=g.a.common.gpu;
        assert(!gt_guard_endpoint(&g,&e) && mmio_reads==reads);g.a.common.pch=pch;
        const struct pci_device *gpu=g.a.common.gpu;g.a.common.gpu=pch;
        assert(!gt_guard_endpoint(&g,&e) && mmio_reads==reads);g.a.common.gpu=gpu;
        g.a.common.cr3=0x2000;assert(!gt_guard_endpoint(&g,&e) && mmio_reads==reads);g.a.common.cr3=0x1000;
        g.a.mapped=true;assert(!gt_guard_endpoint(&g,&e) && mmio_reads==reads);g.a.mapped=false;
        gt_bad_pci=true;assert(gt_guard_endpoint(&g,&e) && e.asls==UINT32_MAX && mmio_reads>reads);
        assert(display_access_release(&g.token,true));reads=mmio_reads;
        assert(!gt_guard_endpoint(&g,&e) && mmio_reads==reads);
        puts("guard-endpoint: PASS fresh full census, adapter binding, direct firmware registers and no reusable authority");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--backing-batch-provider")) {
        reset();backing_mode=true;memory.entry_count=3;
        intel_display_probe_boot(&frame);intel_display_backing_probe_boot(&frame);
        assert(backing.status==1);struct backing_adapter a;assert(trial_adapter(&a));
        assert(backing_map(&a,0x800000));uint64_t values[64];unsigned reads;
        /* Every boundary refusal is before any target load. */
        for(unsigned k=0;k<10;k++) {
            unsigned before=mmio_reads;wake_irq=true;
            uint32_t off=k==0?0x7ffff8:k==1?0x802a60:k==2?0x800001:k==3?0x800ff8:0x800000;
            unsigned n=k==4?0:k==5?65:k==3?2:1;
            if(k==6)wake_irq=false;
            if(k==7)current=0x2000;
            if(k==8)pa[INTEL_CAPTURE_PAGES]^=4096;
            if(k==9)intel_boot_state.client=INTEL_BOOT_LEGACY;
            assert(!backing_pairs(&a,off,n,values,&reads) && !reads && mmio_reads==before);
            current=0x1000;pa[INTEL_CAPTURE_PAGES]=a.physical;intel_boot_state.client=INTEL_BOOT_OBSERVER;
        }
        wake_irq=true;assert(backing_pairs(&a,0x800000,64,values,&reads) && reads==192);
        for(unsigned i=0;i<64;i++)assert(values[i]==0x90000001ull+4096ull*i);
        for(unsigned fault=1;fault<=6;fault++)for(unsigned at=1;at<=192;at+=31) {
            backing_batch_test_fault=fault;backing_batch_test_reads=0;backing_batch_test_at=at;
            bool ok=backing_pairs(&a,0x800000,64,values,&reads);
            if(fault==5 && at%3==2){assert(ok && values[(at-1)/3]!=0x90000001ull+4096ull*((at-1)/3));}
            else assert(!ok);
            assert(reads<=192 && (fault!=1 || reads==at-1));
            current=0x1000;pa[INTEL_CAPTURE_PAGES]=a.physical;wake_irq=true;intel_boot_state.client=INTEL_BOOT_OBSERVER;
        }
        backing_batch_test_fault=0;assert(backing_unmap(&a));wake_irq=false;
        puts("backing-batch-provider: PASS bounds, IRQ/claim/current/exact translation, high-low-high loads, partial failure and post-check refusal");return 0;
    }
#endif
    /* Optional local integration uses an on-disk firmware reference, never
     * real MMIO. The production pin stays enabled; synthetic headers refuse. */
    if(argc==3 && !strcmp(argv[1],"--dma-declared-table")) {
        reset();FILE *f=fopen(argv[2],"rb");assert(f);
        assert(fread(extra_bytes,1,sizeof(extra_bytes),f)==sizeof(extra_bytes) && fgetc(f)==EOF);assert(!fclose(f));
        extra_dmar=(struct acpi_table_info){.physical_address=(uint64_t)(uintptr_t)extra_bytes,.length=136};memcpy(extra_dmar.signature,"DMAR",4);extra_table=true;
        struct limine_memmap_entry lo={.base=0xfed1c000,.length=0x4000,.type=LIMINE_MEMMAP_RESERVED};
        struct limine_memmap_entry hi={.base=0xffd70000,.length=0x30000,.type=LIMINE_MEMMAP_RESERVED};
        struct limine_memmap_entry *map[]={&ram,&aperture,&stolen_region,&lo,&hi};memory.entries=map;memory.entry_count=5;
        assert(dma_resource() && dma_resource_source==DMA_RESOURCE_DECLARED_GAP && dma_resource_declared);
        struct backing_adapter a={.common={.cr3=0x1000}};uint32_t value;
        assert(dma_map(&a,0xfed90000));
        for(unsigned i=0;i<8;i++)assert(dma_read(&a,(uint32_t[]){0,8,12,16,20,28,32,36}[i],&value));
        assert(dma_unmap(&a) && map_calls==1 && unmap_calls==1 && mmio_reads==8);
        extra_bytes[9]^=1;assert(!dma_resource() && !dma_map(&a,0xfed90000) && map_calls==1 && mmio_reads==8);
        extra_bytes[9]^=1;extra_bytes[10]^=1;
        unsigned sum=0;for(unsigned i=0;i<sizeof(extra_bytes);i++)sum+=extra_bytes[i];extra_bytes[9]-=(uint8_t)sum;
        struct intel_dma_topology changed;assert(intel_dma_table(extra_bytes,sizeof(extra_bytes),&changed) && changed.hash!=INTEL_DMA_DMAR_HASH);
        assert(!dma_resource() && !dma_map(&a,0xfed90000) && map_calls==1 && mmio_reads==8);
        puts("declared-MMIO adapter: pinned reference maps one MOCK read-only page, eight allowed reads, clean retirement; changed table refused");return 0;
    }
    if(argc==2 && !strncmp(argv[1],"--dma-resource-",15)) {
        reset();struct limine_memmap_entry units[5];
        struct limine_memmap_entry *map[8]={&ram,&aperture,&stolen_region,&firmware_region};
        memory.entries=map;memory.entry_count=3;
        const char *mode=argv[1]+15;
        unsigned count=!strcmp(mode,"missing")?0:!strcmp(mode,"many")?5:!strcmp(mode,"ambiguous")?2:1;
        for(unsigned i=0;i<count;i++) {
            units[i]=(struct limine_memmap_entry){.base=0xfed90000,.length=4096,.type=LIMINE_MEMMAP_RESERVED};map[3+i]=&units[i];memory.entry_count++;
        }
        if(!strcmp(mode,"conflict"))units[0].type=LIMINE_MEMMAP_USABLE;
        if(!strcmp(mode,"partial"))units[0].length=4095;
        if(!strcmp(mode,"malformed")){units[0].base=UINT64_MAX;units[0].length=2;}
        unsigned maps=map_calls,reads=mmio_reads;assert(!dma_resource());assert(map_calls==maps && mmio_reads==reads);
        dma_checked=63;dma_passed=31;dma_failed=32;dma_guards=1;
        dma_attempted=true;dma_result.status=DMA_ACCESS;reporting=true;assert(!intel_dma_probe_report());return 0;
    }
#ifdef SRVROS_INTEL_ENGINE_TRIAL
    if(argc==2 && !strncmp(argv[1],"--wake-engine-",14))engine_mode=argv[1]+14;
    static char engine_recipe[1024];
    snprintf(engine_recipe,sizeof(engine_recipe),"%s srvros.intel-engines=read-only",wake_cmd.cmdline);
    wake_cmd.cmdline=engine_recipe;
#ifdef SRVROS_INTEL_COMPLETION_TRIAL
    static char completion_recipe[1200];snprintf(completion_recipe,sizeof(completion_recipe),"%s srvros.intel-completion=read-only",engine_recipe);wake_cmd.cmdline=completion_recipe;
#ifdef SRVROS_INTEL_TAKEOVER_PREFLIGHT
    static char takeover_recipe[1400];snprintf(takeover_recipe,sizeof(takeover_recipe),"%s srvros.intel-takeover=preflight",completion_recipe);wake_cmd.cmdline=takeover_recipe;
#ifdef SRVROS_INTEL_REQUESTER_AUDIT
    static char requester_recipe[1600];snprintf(requester_recipe,sizeof(requester_recipe),"%s srvros.intel-requester=read-only",takeover_recipe);wake_cmd.cmdline=requester_recipe;
#endif
#ifdef SRVROS_INTEL_ENGINE_PREPARE
    static char prepare_recipe[1600];snprintf(prepare_recipe,sizeof(prepare_recipe),"%s srvros.intel-engines-prepare=request-cancel",takeover_recipe);wake_cmd.cmdline=prepare_recipe;
#ifdef SRVROS_INTEL_HELD_REQUESTER
    static char held_recipe[1800];snprintf(held_recipe,sizeof(held_recipe),"%s srvros.intel-held-requester=read-only",prepare_recipe);wake_cmd.cmdline=held_recipe;
#ifdef SRVROS_INTEL_PREPARE_RESET
    static char held_reset_recipe[1900];snprintf(held_reset_recipe,sizeof(held_reset_recipe),"%s srvros.intel-bcs-reset=held-blitter-only",held_recipe);wake_cmd.cmdline=held_reset_recipe;
#ifdef SRVROS_INTEL_HELD_INTERVAL
    static char interval_recipe[2000];snprintf(interval_recipe,sizeof interval_recipe,"%s srvros.intel-interval=read-only",held_reset_recipe);wake_cmd.cmdline=interval_recipe;
#ifdef SRVROS_INTEL_REHOLD
    static char rehold_recipe[2100];snprintf(rehold_recipe,sizeof rehold_recipe,"%s srvros.intel-rehold=request-cancel",interval_recipe);wake_cmd.cmdline=rehold_recipe;
#ifdef SRVROS_INTEL_JOINT
    static char joint_recipe[2200];snprintf(joint_recipe,sizeof joint_recipe,"%s srvros.intel-joint=hda-claim",rehold_recipe);wake_cmd.cmdline=joint_recipe;
#ifdef SRVROS_INTEL_GPU_HOLD
    static char gpu_hold_recipe[2300];snprintf(gpu_hold_recipe,sizeof gpu_hold_recipe,"%s srvros.intel-gpu-requester=hold",joint_recipe);wake_cmd.cmdline=gpu_hold_recipe;
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#ifdef SRVROS_INTEL_BCS_RESET_TRIAL
    static char reset_recipe[1400];snprintf(reset_recipe,sizeof(reset_recipe),"%s srvros.intel-bcs-reset=blitter-only",completion_recipe);wake_cmd.cmdline=reset_recipe;
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
    static char reset_shutdown_recipe[1600];snprintf(reset_shutdown_recipe,sizeof(reset_shutdown_recipe),"%s srvros.intel-shutdown=observe-disabled",reset_recipe);wake_cmd.cmdline=reset_shutdown_recipe;
#endif
#endif
#ifdef SRVROS_INTEL_RESET_READY_TRIAL
    static char ready_recipe[1400];snprintf(ready_recipe,sizeof(ready_recipe),"%s srvros.intel-reset-ready=request-cancel",completion_recipe);wake_cmd.cmdline=ready_recipe;
#endif
#endif
#ifdef SRVROS_INTEL_BLITTER_TRIAL
    bcs_entry_disabled=!strncmp(engine_mode,"blit-off-",9);if(bcs_entry_disabled)bcs_ctl=0;
    static char blitter_recipe[1200];snprintf(blitter_recipe,sizeof(blitter_recipe),"%s srvros.intel-blitter=retire",engine_recipe);wake_cmd.cmdline=blitter_recipe;
#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
    static char shutdown_recipe[1400];snprintf(shutdown_recipe,sizeof(shutdown_recipe),"%s srvros.intel-shutdown=qualify",blitter_recipe);wake_cmd.cmdline=shutdown_recipe;
#ifdef SRVROS_INTEL_HDA_SHUTDOWN
    static char hda_shutdown_recipe[1600];snprintf(hda_shutdown_recipe,sizeof hda_shutdown_recipe,"%s srvros.intel-hda-bcs=joint-owner-v1",shutdown_recipe);wake_cmd.cmdline=hda_shutdown_recipe;
#endif
#endif
#endif
#endif
    reset();wake_mode=argc==2 && !strncmp(argv[1],"--wake-",7);wake_cache_fail=wake_mode && !strcmp(argv[1],"--wake-cache");wake_map_fail=wake_mode && !strcmp(argv[1],"--wake-map");wake_retire_fail=wake_mode && !strcmp(argv[1],"--wake-retire");wake_uncertain=wake_mode && !strcmp(argv[1],"--wake-uncertain");wake_no_ack=wake_mode && !strcmp(argv[1],"--wake-no-ack");wake_foreign=wake_mode && !strcmp(argv[1],"--wake-foreign");wake_busy=wake_mode && !strcmp(argv[1],"--wake-busy");wake_thread_stuck=wake_mode && !strcmp(argv[1],"--wake-c0");wake_late_foreign=wake_mode && !strcmp(argv[1],"--wake-late-foreign");wake_fw_after=wake_mode && !strcmp(argv[1],"--wake-fw-after");fw_mode=wake_mode || (argc==2 && !strncmp(argv[1],"--fw-",5));fw_bad_signature=fw_mode && !strcmp(argv[1],"--fw-signature");fw_changed=fw_mode && !strcmp(argv[1],"--fw-changed");fw_cache_fail=fw_mode && !strcmp(argv[1],"--fw-cache");fw_map_fail=fw_mode && !strcmp(argv[1],"--fw-map");fw_retire_fail=fw_mode && !strcmp(argv[1],"--fw-retire");gt_mode=fw_mode || (argc==2 && !strncmp(argv[1],"--gt-",5));gt_cache_fail=gt_mode && !strcmp(argv[1],"--gt-cache-failed");gt_map_fail=gt_mode && !strcmp(argv[1],"--gt-map-failed");gt_retire_fail=gt_mode && !strcmp(argv[1],"--gt-retire-failed");gt_changed=gt_mode && !strcmp(argv[1],"--gt-changed");gt_bad_pci=gt_mode && !strcmp(argv[1],"--gt-pci-failed");gt_float=gt_mode && !strcmp(argv[1],"--gt-float-read");window_mode=gt_mode || (argc==2 && !strncmp(argv[1],"--window-",9));window_map_fail=window_mode && !strcmp(argv[1],"--window-map-failed");window_retire_fail=window_mode && !strcmp(argv[1],"--window-retire-failed");window_cache_fail=window_mode && !strcmp(argv[1],"--window-cache-refused");cache_mode=window_mode || (argc==2 && !strcmp(argv[1],"--cache-backing-report"));backing_mode=cache_mode || (argc==2 && !strcmp(argv[1],"--backing-report"));reporting=argc==2 && (!strcmp(argv[1],"--report") || backing_mode);if(window_mode){stolen_region.base=0x8c000000;stolen_region.length=0x4000000;}
    if(backing_mode)memory.entry_count=3;
    if(fw_mode){ram.length=0x10000000;firmware_region=(struct limine_memmap_entry){.base=0x12345000,.length=0x3000,.type=3};memory.entry_count=4;if(!strcmp(argv[1],"--fw-usable"))firmware_region.type=0;if(!strcmp(argv[1],"--fw-short"))firmware_region.length=4096;}
#ifdef SRVROS_INTEL_PREPARE_RESET
    if(wake_mode)console_printf("cmdline: %s\n",wake_cmd.cmdline);
#endif
    intel_display_probe_boot(&frame);
    assert(result.status==INTEL_CAPTURE_OBSERVED && map_calls==9 && unmap_calls==9 && mmio_reads==116);
    assert(intel_display_probe_report());unsigned old_reads=mmio_reads;intel_display_probe_boot(&frame);assert(mmio_reads==old_reads);
    if(backing_mode){intel_display_backing_probe_boot(&frame);assert(backing.status==1 && backing.count==1356 && backing.sample[0].present==1356 && backing.fence_matches==1 && !backing.retained && map_calls==unmap_calls);struct scanout_profile profile;struct intel_scanout_clock clock;assert(intel_scanout_trial_profile(&profile) && !profile.assured && !profile.physical[1]);assert(intel_scanout_trial_ram(0x1000000,1350));assert(!intel_scanout_trial_ram(0x8c000000,1350));assert(!intel_scanout_trial_ram(0xb0000000,1350));assert(intel_scanout_trial_clock(&clock));assert(intel_scanout_trial_revalidate());uint64_t pages[3]={0};assert(intel_scanout_trial_cache_pages(pages)==cache_mode);if(cache_mode)assert(pages[0]==0xc0101000 && pages[1]==0xc0811000 && pages[2]==0xb2200000);assert(intel_display_backing_probe_report());if(window_mode){assert(intel_ggtt_window_probe_report()==!(window_map_fail || window_retire_fail || window_cache_fail));assert((pa[INTEL_CAPTURE_PAGES]!=0)==(window_map_fail || window_retire_fail));if(!window_map_fail && !window_retire_fail && !window_cache_fail){struct scanout_handoff_snapshot s;assert(intel_window_handoff_snapshot(&s) && !s.assurances);config_reads=0;assert(intel_window_handoff_revalidate(&s) && intel_window_handoff_clean());unsigned old=config_reads;s.original[15]^=1;assert(!intel_window_handoff_revalidate(&s) && config_reads==old);s.original[15]^=1;config_reads=0;handoff_drift=true;assert(!intel_window_handoff_revalidate(&s) && intel_window_handoff_clean() && !pa[INTEL_CAPTURE_PAGES]);handoff_drift=false;config_reads=0;window_cache_fail=true;assert(!intel_window_handoff_revalidate(&s) && intel_window_handoff_clean() && !pa[INTEL_CAPTURE_PAGES]);window_cache_fail=false;}}if(gt_mode){config_reads=0;bool ok=intel_gt_preflight_probe_report();assert(ok==!(gt_cache_fail || gt_map_fail || gt_retire_fail || gt_changed || gt_bad_pci || gt_float));assert((pa[INTEL_CAPTURE_PAGES]!=0)==(gt_map_fail || gt_retire_fail));assert(display_access_allowed()==!(gt_map_fail || gt_retire_fail));}if(fw_mode){config_reads=0;assert(intel_gt_firmware_probe_report()==!(fw_bad_signature || fw_changed || fw_cache_fail || fw_map_fail || fw_retire_fail || !strcmp(argv[1],"--fw-usable") || !strcmp(argv[1],"--fw-short")));assert((pa[INTEL_CAPTURE_PAGES]!=0)==(fw_map_fail || fw_retire_fail));assert(display_access_allowed()==!(fw_map_fail || fw_retire_fail));}
#ifdef SRVROS_INTEL_WAKE_TRIAL
 if(wake_mode){config_reads=0;if(!strcmp(argv[1],"--wake-loader"))wake_loader.version="unknown";if(!strcmp(argv[1],"--wake-efi"))wake_firmware.firmware_type=1;if(!strcmp(argv[1],"--wake-recipe"))wake_cmd.cmdline="srvros.intel-wake=trial";bool good=!(wake_cache_fail || wake_map_fail || wake_retire_fail || wake_uncertain || wake_no_ack || wake_foreign || wake_busy || wake_thread_stuck || wake_late_foreign || wake_fw_after || !strcmp(argv[1],"--wake-loader") || !strcmp(argv[1],"--wake-efi") || !strcmp(argv[1],"--wake-recipe"));
#ifdef SRVROS_INTEL_ENGINE_TRIAL
 good=good && strcmp(engine_mode,"float-last") && strcmp(engine_mode,"read-fail-last") && strcmp(engine_mode,"float") && strcmp(engine_mode,"read-fail") && strcmp(engine_mode,"cache-last") && strcmp(engine_mode,"map-last") && strcmp(engine_mode,"retire-last") && strcmp(engine_mode,"late-foreign") && strcmp(engine_mode,"slow") && strcmp(engine_mode,"frozen");
#endif
#ifdef SRVROS_INTEL_COMPLETION_TRIAL
 good=good && strcmp(engine_mode,"changed") && strcmp(engine_mode,"context-change") && strcmp(engine_mode,"context-read") && strcmp(engine_mode,"context-float") && strcmp(engine_mode,"tlb-float") && strcmp(engine_mode,"tlb-reserved") && strcmp(engine_mode,"tlb-read") && strcmp(engine_mode,"tlb-cache") && strcmp(engine_mode,"tlb-map") && strcmp(engine_mode,"tlb-retire");
#endif
#ifdef SRVROS_INTEL_BLITTER_TRIAL
#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
 good=good && strcmp(engine_mode,"active") && strcmp(engine_mode,"pending") && strcmp(engine_mode,"changed");
#endif
 if(!strncmp(engine_mode,"blit-",5))good=good && (!strcmp(engine_mode,"blit-report") || !strcmp(engine_mode,"blit-off-report"));
 if(bcs_entry_disabled)assert(!bcs_writes);
#endif
#ifdef SRVROS_INTEL_RESET_READY_TRIAL
 good=good && (!strcmp(engine_mode,"report") || !strcmp(engine_mode,"ready-delay") || !strcmp(engine_mode,"ready-mask"));
#endif
#ifdef SRVROS_INTEL_BCS_RESET_TRIAL
 good=good && !strcmp(engine_mode,"report");
#endif
#ifdef SRVROS_INTEL_TAKEOVER_PREFLIGHT
 if(!strncmp(engine_mode,"take-",5))good=good && (!strcmp(engine_mode,"take-busy") || !strcmp(engine_mode,"take-mask") || !strcmp(engine_mode,"take-reserved") || !strcmp(engine_mode,"take-domain") || !strcmp(engine_mode,"take-domain-reserved"));
#endif
#ifdef SRVROS_INTEL_ENGINE_PREPARE
#ifdef SRVROS_INTEL_HELD_REQUESTER
 good=good && (!strcmp(engine_mode,"report") || !strcmp(engine_mode,"held-pending") || !strcmp(engine_mode,"held-pending-transition") || !strcmp(engine_mode,"held-master-off")
#ifdef SRVROS_INTEL_PREPARE_RESET
 || !strcmp(engine_mode,"hreset-delay")
#endif
 );
#else
 good=good && !strcmp(engine_mode,"report");
#endif
#endif
#ifdef SRVROS_INTEL_REQUESTER_AUDIT
 good=good && (!strcmp(engine_mode,"report") || !strcmp(engine_mode,"req-pending") || !strcmp(engine_mode,"req-pending-transition") || !strcmp(engine_mode,"take-busy") || !strcmp(engine_mode,"take-mask") || !strcmp(engine_mode,"take-reserved") || !strcmp(engine_mode,"take-domain") || !strcmp(engine_mode,"take-domain-reserved") || !strcmp(engine_mode,"active") || !strcmp(engine_mode,"pending") || !strcmp(engine_mode,"flip-wait") || !strcmp(engine_mode,"tlb-pending"));
#endif

#ifdef SRVROS_INTEL_GPU_HOLD
if(!strcmp(engine_mode,"gpu-pending"))good=true;
#endif
#ifdef SRVROS_INTEL_HDA_SHUTDOWN
if(!strncmp(engine_mode,"joint-",6) || !strncmp(engine_mode,"paired-",7))good=false;
#endif
assert(intel_gt_wake_probe_report()==good);assert(!wake_irq);assert(intel_gt_wake_retained()==!display_access_allowed());
#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
shutdown_check_lifetime();
#endif
#ifdef SRVROS_INTEL_BLITTER_TRIAL
 if(!strcmp(engine_mode,"blit-uncertain") || !strcmp(engine_mode,"blit-readback") || !strcmp(engine_mode,"blit-late-foreign") || !strcmp(engine_mode,"blit-reader-change") || !strcmp(engine_mode,"blit-audio-change") || !strcmp(engine_mode,"blit-mode-change")) {
  assert(!display_access_allowed());for(unsigned j=0;j<WAKE_MAPS;j++)assert(pa[INTEL_CAPTURE_PAGES+1+j]);
 }
#endif
if(good){assert(wake_writes==3 && !wake_ack && (wake_request&0xffff)==0 && display_access_allowed());}}
#else
 if(wake_mode){assert(!intel_gt_wake_probe_report() && !wake_writes && display_access_allowed());}
#endif
 unsigned calls=mmio_reads;intel_display_backing_probe_boot(&frame);assert(calls==mmio_reads);return 0;}
    if(reporting)return 0;
    reset();firmware_ok=false;intel_display_probe_boot(&frame);assert(result.status==INTEL_CAPTURE_IDENTITY && !map_calls && !mmio_reads);
    reset();ram.base=0xc0000000;intel_display_probe_boot(&frame);assert(result.status==INTEL_CAPTURE_ACCESS && !map_calls && !mmio_reads);
    reset();malformed=true;intel_display_probe_boot(&frame);assert(result.status==INTEL_CAPTURE_ACCESS && !map_calls);
    reset();clock_ok=false;intel_display_probe_boot(&frame);assert(result.status==INTEL_CAPTURE_CLOCK && !config_reads && !map_calls);
    reset();current=0x2000;intel_display_probe_boot(&frame);assert(result.status==INTEL_CAPTURE_CLOCK && !map_calls);
    reset();pa[3]=0xc0062000;intel_display_probe_boot(&frame);assert(result.status==INTEL_CAPTURE_MAP && map_calls==3 && unmap_calls==3 && !mmio_reads && pa[3]==0xc0062000);
    reset();backing_mode=true;memory.entry_count=2;intel_display_probe_boot(&frame);translation_gap=true;intel_display_backing_probe_boot(&frame);assert(backing.status==2 && map_calls==9 && unmap_calls==9);
    reset();backing_mode=true;memory.entry_count=2;intel_display_probe_boot(&frame);pa[INTEL_CAPTURE_PAGES]=0xc0100000;intel_display_backing_probe_boot(&frame);assert(backing.status!=1 && map_calls==9 && pa[INTEL_CAPTURE_PAGES]);
    reset();
    unsigned before=map_calls;intel_dma_probe_boot();assert(dma_result.status==DMA_REFUSED && map_calls==before);
    reporting=argc==2 && !strcmp(argv[1],"--dma-refused");
    assert(!intel_dma_probe_report());if(reporting)return 0;
    struct limine_memmap_entry unit={.base=0xfed90000,.length=4096,.type=LIMINE_MEMMAP_RESERVED};
    struct limine_memmap_entry *dma_entries[]={&ram,&aperture,&stolen_region,&unit};memory.entries=dma_entries;memory.entry_count=4;
    struct backing_adapter a={.common={.cr3=0x1000}};uint32_t value;
    a.mapped=true;assert(!dma_guard(&a) && dma_checked==4 && dma_failed==4);
    a.mapped=false;trial_mapping_fault=true;assert(!dma_guard(&a) && dma_checked==12 && dma_failed==8);
    trial_mapping_fault=false;current=0x2000;assert(!dma_guard(&a) && dma_checked==28 && dma_failed==16);
    current=0x1000;assert(!dma_guard(&a) && dma_checked==29 && dma_failed==1);
    assert(!map_calls && !mmio_reads && !dma_memory.checked);
    assert(dma_resource() && dma_map(&a,0xfed90000));
    assert(!dma_map(&a,0xfed90000) && !dma_read(&a,24,&value) && !dma_read(&a,52,&value));
    for(unsigned i=0;i<8;i++){uint32_t off=(uint32_t[]){0,8,12,16,20,28,32,36}[i];assert(dma_read(&a,off,&value) && value==0x1000000+off);}
    assert(dma_unmap(&a) && !a.mapped && !pa[INTEL_CAPTURE_PAGES]);
    /* The exact captured gap needs a separately verified declaration. A host
     * synthetic/absent table can select policy in isolation but cannot map it. */
    struct limine_memmap_entry before_gap={.base=0xfed1c000,.length=0x4000,.type=LIMINE_MEMMAP_RESERVED};
    struct limine_memmap_entry after_gap={.base=0xffd70000,.length=0x30000,.type=LIMINE_MEMMAP_RESERVED};
    struct limine_memmap_entry *gap_entries[]={&ram,&aperture,&stolen_region,&before_gap,&after_gap};
    memory.entries=gap_entries;memory.entry_count=5;unsigned old_maps=map_calls,old_mmio=mmio_reads;
    assert(!dma_resource() && !dma_resource_select(false) && dma_resource_select(true)==DMA_RESOURCE_DECLARED_GAP);
    assert(!dma_map(&a,0xfed90000) && map_calls==old_maps && mmio_reads==old_mmio && !dma_resource_declared);
    before_gap.base+=4096;assert(!dma_resource() && !dma_resource_select(true));before_gap.base-=4096;
    after_gap.length-=4096;assert(!dma_resource() && !dma_resource_select(true));after_gap.length+=4096;
    before_gap.type=LIMINE_MEMMAP_USABLE;assert(!dma_resource() && !dma_resource_select(true));before_gap.type=LIMINE_MEMMAP_RESERVED;
    gap_entries[2]=NULL;assert(!dma_resource() && !dma_resource_select(true));gap_entries[2]=&stolen_region;
    gap_entries[2]=&unit;
    for(uint64_t type=0;type<=8;type++) {
        unit.type=type;unit.length=4095;
        assert(!dma_resource() && !dma_resource_select(true));
    }
    unit.type=LIMINE_MEMMAP_RESERVED;unit.length=4096;
    memory.entries=dma_entries;memory.entry_count=4;
    unit.type=LIMINE_MEMMAP_USABLE;assert(!dma_resource() && !dma_map(&a,0xfed90000));
    unit.type=LIMINE_MEMMAP_RESERVED;unit.length=4095;assert(!dma_resource());
    unit.length=4096;
    assert(dma_map(&a,0xfed90000));pa[INTEL_CAPTURE_PAGES]+=4096;
    assert(!dma_read(&a,0,&value));pa[INTEL_CAPTURE_PAGES]=0xfed90000;
    dma_unmap_fail=true;assert(!dma_unmap(&a) && trial_mapping_fault && a.mapped);
    assert(!dma_map(&a,0xfed90000));dma_unmap_fail=false;
    assert(backing_unmap(&a) && !dma_map(&a,0xfed90000)); /* Host cleanup cannot clear sticky fault. */
    assert(!dma_table_current());
    for(unsigned policy=0;policy<=3;policy++)if(policy!=INTEL_BOOT_OBSERVER) {
        reset();memset(&intel_boot_state,0,sizeof(intel_boot_state));
        if(policy)assert(intel_boot_select(policy));
        if(policy==INTEL_BOOT_LEGACY)assert(intel_boot_claim(policy,&devices[0]));
        intel_display_probe_boot(&frame);
        assert(result.status==INTEL_CAPTURE_IDENTITY && !config_reads && !map_calls && !mmio_reads);
    }
    reset();assert(intel_boot_claim(INTEL_BOOT_OBSERVER,&devices[0]));
    struct adapter lost={.gpu=&devices[0],.pch=&devices[1],.cr3=0x1000};
    assert(map_page(&lost,0,0xc0000000ull+intel_capture_pages[0]));
    devices[0].bar[0]^=4096;
    struct intel_capture_identity denied;
    assert(!identity(&lost,&denied) && !read_word(&lost,intel_capture_pages[0],&value));
    assert(!map_page(&lost,1,0xc0000000ull+intel_capture_pages[1]));
    assert(unmap_page(&lost,0) && !pa[0] && map_calls==1 && unmap_calls==1 && !mmio_reads);
    puts("intel-display-probe: PASS actual adapter, read-only temporary mappings, existing-map/RAM alias refusal, firmware/PM/clock gates, boot-client refusal and cleanup after lost resource identity");
    return 0;
}

#if defined(SRVROS_INTEL_RESET_READY_TRIAL) || defined(SRVROS_INTEL_BCS_RESET_TRIAL)
static uint32_t ready_control;
static bool intel_display_test_reset_ready_read(uint32_t reg,uint32_t *out) {
 assert(wake_irq && wake_ack==1 && (wake_request&65535)==1 && !display_access_allowed());
 if(reg==0x220d0) {
  ready_reads++;ready_phase_reads++;
  if(!strcmp(engine_mode,"ready-read") || (!strcmp(engine_mode,"step-control-read") && ready_writes))return false;
  if(!strcmp(engine_mode,"ready-initial" ) && !ready_writes)ready_control=1;
  if(!strcmp(engine_mode,"ready-delay") && ready_writes==1 && ready_phase_reads==3)ready_control=3;
  if(!strcmp(engine_mode,"ready-reserved"))ready_control=4;
  if(!strcmp(engine_mode,"ready-float"))ready_control=UINT32_MAX;
  if(!strcmp(engine_mode,"ready-late-read") && ready_writes==2)return false;
  *out=ready_control;return true;
 }
 static const uint32_t regs[8]={0x22030,0x22034,0x22038,0x2203c,0x22234,0x22238,0x2209c,0x2229c};
 unsigned i=ready_group_words++%8;assert(reg==regs[i]);
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
 if(!ready_writes && ready_group_words<=32) {
  if(!strcmp(engine_mode,"shut-read"))return false;
  if(!strcmp(engine_mode,"shut-foreign") && i==7)wake_request|=2;
  if(!strcmp(engine_mode,"shut-clock"))ticks+=100001;
  if(!strcmp(engine_mode,"shut-backward"))ticks=1;
 }
#endif

 *out=i==4?1:i==6?512:0;
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
 if(!ready_writes && ready_group_words<=32) {
  if(!strcmp(engine_mode,"shut-float"))*out=UINT32_MAX;
  if(!strcmp(engine_mode,"shut-start") && i==2)*out=0x3eef000;
  if(!strcmp(engine_mode,"shut-enabled") && i==3)*out=0xf001;
  if(!strcmp(engine_mode,"shut-disabled-geometry") && i==3)*out=0xf000;
  if(!strcmp(engine_mode,"shut-mode") && i==6)*out|=1u<<16;
  if(!strcmp(engine_mode,"shut-drift") && ready_group_words==16)*out=1;
 }
#endif

 if(!strcmp(engine_mode,"ready-busy") && i==0)*out=8;
 if(!strcmp(engine_mode,"ready-drift") && ready_writes==2 && i==7)*out=1;
 return true;
}
static enum intel_gt_write intel_display_test_reset_ready_write(uint32_t reg,uint32_t value) {
 assert(reg==0x220d0 && wake_irq && wake_ack==1 && !display_access_allowed());
 assert(value==(ready_writes?0x10000u:0x10001u));assert(ready_writes<2);
 if(!strcmp(engine_mode,"ready-none") || (!strcmp(engine_mode,"ready-cancel-none") && ready_writes==1))return GT_WRITE_NONE;
 ready_writes++;if(!strcmp(engine_mode,"step-phase"))ticks+=10001;ready_phase_reads=0;ready_control=(ready_writes==1?3:0)|(!strcmp(engine_mode,"ready-mask")?(value&0xffff0000u):0);
 if(ready_writes==1 && (!strcmp(engine_mode,"ready-timeout") || !strcmp(engine_mode,"ready-delay")))ready_control=1;
 if(ready_writes==2 && !strcmp(engine_mode,"ready-cancel-timeout"))ready_control=2;
 if(!strcmp(engine_mode,"ready-foreign"))wake_request|=2;
 return !strcmp(engine_mode,"ready-uncertain")?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
#endif

#ifdef SRVROS_INTEL_BCS_RESET_TRIAL
static uint64_t intel_display_test_bcs_now(void) {
 uint64_t t=timer_monotonic_us();
 if(bcs_reset_writes==2 && ready_writes==1 && bcs_domain_observations==3 && ++core_ack_clocks>=2) {
  if(!strcmp(engine_mode,"core-settle-deadline"))return t+100001;
  if(!strcmp(engine_mode,"core-settle-zero"))return 0;
  if(!strcmp(engine_mode,"core-settle-backward"))return 1;
 }
 return t;
}
static bool intel_display_test_bcs_read(uint32_t reg,uint32_t *out) {
 assert(wake_irq && wake_ack==1 && !display_access_allowed());
 if(reg==0x941c){if(!strcmp(engine_mode,"step-domain-read") && bcs_reset_writes)return false;++bcs_domain_observations;if(!strcmp(engine_mode,"bcs-float")){*out=UINT32_MAX;return true;}*out=!strcmp(engine_mode,"bcs-initial")?8:!strcmp(engine_mode,"bcs-foreign")?2:((!strcmp(engine_mode,"bcs-timeout") && bcs_reset_writes) || (!strcmp(engine_mode,"bcs-second-timeout") && bcs_reset_writes==2))?8:0;return strcmp(engine_mode,"bcs-read")!=0;}
 static const uint32_t offsets[9]={0x180,0x19c,0x1ac,0x80,0x84,0xac,0x280,0x29c,0x2ac};
 unsigned p=(reg-0x70000)/4096,i=bcs_plane_calls++%9;assert(p<3 && reg==0x70000+p*4096+offsets[i]);
 *out=(!p && !i)?0xd8000400u:0;
 if(bcs_reset_writes && !strcmp(engine_mode,"bcs-plane") && i==8)*out=1;
 return !(bcs_reset_writes && !strcmp(engine_mode,"bcs-plane-read"));
}
static enum intel_gt_write intel_display_test_bcs_write(uint32_t reg,uint32_t value) {
 assert(reg==0x941c && value==8 && wake_irq && wake_ack==1 && !display_access_allowed() && bcs_reset_writes<2);
 ++bcs_reset_writes;
 if(!strcmp(engine_mode,"step-foreign"))wake_request|=2;
 if(!strcmp(engine_mode,"bcs-none") || (!strcmp(engine_mode,"bcs-second-none") && bcs_reset_writes==2))return GT_WRITE_NONE;
 ready_control=0;
 return (!strcmp(engine_mode,"bcs-uncertain") || (!strcmp(engine_mode,"bcs-second-uncertain") && bcs_reset_writes==2))?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
#endif

#ifdef SRVROS_INTEL_BCS_RESET_TRIAL
static bool intel_display_test_wake_snapshot(uint32_t *r,uint32_t *a,uint32_t *t) {
 assert(wake_irq && !display_access_allowed());
 if(!intel_display_test_wake_read(INTEL_GT_REQUEST,r) || !intel_display_test_wake_read(INTEL_GT_ACK,a) || !intel_display_test_wake_read(INTEL_GT_THREAD,t))return false;
 if(bcs_reset_writes==2 && ready_writes==2){
  if(!strcmp(engine_mode,"snap-read"))return false;
  if(!strcmp(engine_mode,"snap-foreign"))*r|=2;
  if(!strcmp(engine_mode,"snap-total"))ticks+=300001;
  if(!strcmp(engine_mode,"snap-phase"))ticks+=100001;
  if(!strcmp(engine_mode,"snap-backward"))ticks=1;
 }
 return true;
}
#endif

#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
static bool intel_display_test_wake_snapshot(uint32_t *r,uint32_t *a,uint32_t *t) {
 return intel_display_test_wake_read(INTEL_GT_REQUEST,r) && intel_display_test_wake_read(INTEL_GT_ACK,a) && intel_display_test_wake_read(INTEL_GT_THREAD,t);
}
#endif

#ifdef SRVROS_INTEL_REQUESTER_AUDIT
static bool intel_display_test_requester_read(unsigned i,uint32_t reg,uint32_t *out) {
 assert(i<9 && reg==intel_pci_requester_offsets[i] && wake_irq && wake_ack==1 && !display_access_allowed());
 ++requester_reads;
 if(!strncmp(engine_mode,"req-read:",9) && requester_reads==(unsigned)atoi(engine_mode+9))return false;
 *out=pci_read_config32(&devices[0],(uint8_t)reg);
 if(!strcmp(engine_mode,"req-status-reserved") && i==8)*out|=512;
 if(!strncmp(engine_mode,"req-float:",10) && requester_reads==(unsigned)atoi(engine_mode+10))*out=UINT32_MAX;
 if(!strncmp(engine_mode,"req-profile:",12) && i==(unsigned)atoi(engine_mode+12))*out^=1;
 if(!strncmp(engine_mode,"req-foreign:",12) && requester_reads==(unsigned)atoi(engine_mode+12))wake_request|=2;
 if(!strncmp(engine_mode,"req-map:",8) && requester_reads==(unsigned)atoi(engine_mode+8))pa[INTEL_CAPTURE_PAGES+1+1]^=4096;
 return true;
}
#endif

#ifdef SRVROS_INTEL_HELD_REQUESTER
static void intel_display_test_held_boundary(bool exit) {
 assert(wake_irq && wake_ack==1 && !display_access_allowed() && prepare_stores==5);
 assert(held_boundary==(exit?1u:0u));held_boundary=exit?2:1;
}
static bool intel_display_test_held_read(uint32_t reg,uint32_t *out) {
 unsigned i=held_reads%INTEL_PCI_REQUESTER_WORDS;
 assert(held_boundary==1 && reg==intel_pci_requester_offsets[i] && wake_irq && wake_ack==1 && !display_access_allowed());
 assert(prepare_stores==5);
 for(unsigned e=0;e<5;e++)assert(prepare_controls[e]==3);
 unsigned at=++held_reads;const char *colon=strchr(engine_mode,':');unsigned point=colon?(unsigned)atoi(colon+1):0;
 if(!strncmp(engine_mode,"held-read:",10) && at==point)return false;
 *out=pci_read_config32(&devices[0],(uint8_t)reg);
 if(!strcmp(engine_mode,"held-master-off") && i==1)*out&=~4u;
 if((!strcmp(engine_mode,"held-pending") || (!strcmp(engine_mode,"held-pending-transition") && at>9)) && i==8)*out=256;
 if(!strncmp(engine_mode,"held-float:",11) && at==point)*out=UINT32_MAX;
 if(!strncmp(engine_mode,"held-profile:",13) && at==point)*out^=i==1?2u:1u;
 if(!strncmp(engine_mode,"held-foreign:",13) && at==point)wake_request|=2;
 if(!strncmp(engine_mode,"held-map:",9) && at==point)pa[INTEL_CAPTURE_PAGES+1+1]^=4096;
 if(!strncmp(engine_mode,"held-time:",10) && at==point)ticks+=100001;
 return true;
}
#endif

#ifdef SRVROS_INTEL_PREPARE_RESET
static bool held_reset_at(const char *mode,unsigned at) {
 const char *colon=strchr(engine_mode,':');
 return colon && (size_t)(colon-engine_mode)==strlen(mode) &&
  !strncmp(engine_mode,mode,strlen(mode)) && at==(unsigned)atoi(colon+1);
}
static void intel_display_test_held_reset_boundary(bool exit) {
 assert(wake_irq && wake_ack==1 && !display_access_allowed() && prepare_stores==5);
 assert(held_boundary==2 && held_reads==18);
 assert(held_reset_boundary==(exit?1u:0u));
 if(exit)assert(held_reset_writes==2 && held_reset_reads==18);
 else {for(unsigned e=0;e<5;e++)assert(prepare_controls[e]==3);}
 held_reset_boundary=exit?2:1;
}
static bool intel_display_test_held_reset_snapshot_read(unsigned index,uint32_t reg,uint32_t *out) {
 static const uint32_t regs[9]={0x22030,0x22034,0x22038,0x2203c,0x22234,0x22238,0x2209c,0x2229c,0x941c};
 static const uint32_t words[9]={0,0,0,0,1,0,512,0,0};
 assert(index<9 && reg==regs[index] && index==held_reset_reads%9);
 assert(held_reset_boundary==1 && held_boundary==2 && held_reads==18 && prepare_stores==5);
 assert(wake_irq && wake_ack==1 && !display_access_allowed());
 unsigned at=++held_reset_reads;
 if(held_reset_at("hreset-read",at))return false;
 *out=words[index];
 if(held_reset_at("hreset-float",at))*out=UINT32_MAX;
 if(held_reset_at("hreset-geometry",at))*out^=1;
 if(held_reset_at("hreset-foreign",at))wake_request|=2;
 if(held_reset_at("hreset-map",at))pa[INTEL_CAPTURE_PAGES+1+9]^=4096;
 if(held_reset_at("hreset-time",at))ticks+=100001;
 if(!strcmp(engine_mode,"hreset-initial") && at==9)*out=8;
 if(!strcmp(engine_mode,"hreset-post-domain") && at==18)*out=8;
 return true;
}
static bool intel_display_test_held_reset_domain(uint32_t *out) {
 assert(held_reset_boundary==1 && held_reset_attempts && prepare_stores==5);
 assert(wake_irq && wake_ack==1 && !display_access_allowed());
 ++held_reset_domain_reads;
 *out=!strcmp(engine_mode,"hreset-timeout")?8:0;
 if(!strcmp(engine_mode,"hreset-domain-time"))ticks+=10001;
 return strcmp(engine_mode,"hreset-domain-read")!=0;
}
static void intel_display_test_held_reset_admission(void) {
 assert(held_reset_boundary==1 && held_reset_reads==9 && prepare_stores==5);
 if(!strcmp(engine_mode,"hreset-admission-time"))ticks+=100001;
 if(!strcmp(engine_mode,"hreset-admission-zero"))held_reset_clock_fault=1;
 if(!strcmp(engine_mode,"hreset-admission-backward"))held_reset_clock_fault=2;
}
static enum intel_gt_write intel_display_test_held_reset_transition(uint32_t value,uint32_t *ack) {
#ifdef SRVROS_INTEL_JOINT
 assert(intel_display_test_joint_inhibited());
#endif
 assert(value==8 && wake_irq && wake_ack==1 && !display_access_allowed());
 assert(held_reset_boundary==1 && held_boundary==2 && held_reads==18 && prepare_stores==5);
 assert(held_reset_reads==9 && held_reset_attempts<2);
 ++held_reset_attempts;
 if(held_reset_at("hreset-none",held_reset_attempts))return GT_WRITE_NONE;
 ++held_reset_writes;
 if(strcmp(engine_mode,"hreset-kept-bcs"))prepare_controls[4]=0;
 if(!strcmp(engine_mode,"hreset-post-other"))prepare_controls[2]=0;
 *ack=(!strcmp(engine_mode,"hreset-delay") || !strcmp(engine_mode,"hreset-timeout") || !strcmp(engine_mode,"hreset-domain-read") || !strcmp(engine_mode,"hreset-domain-time"))?8:0;
 if(held_reset_at("hreset-ack-float",held_reset_attempts))*ack=UINT32_MAX;
 if(held_reset_at("hreset-ack-foreign",held_reset_attempts))*ack=2;
 if(held_reset_at("hreset-store-foreign",held_reset_attempts))wake_request|=2;
 if(held_reset_at("hreset-store-map",held_reset_attempts))pa[INTEL_CAPTURE_PAGES+1+9]^=4096;
 if(held_reset_at("hreset-store-time",held_reset_attempts))ticks+=100001;
 if(held_reset_at("hreset-phase-time",held_reset_attempts))ticks+=10001;
 return held_reset_at("hreset-uncertain",held_reset_attempts)?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
#endif

#ifdef SRVROS_INTEL_HELD_INTERVAL
static void intel_display_test_interval_admission(void) {
 if(!strcmp(engine_mode,"interval-admission-time"))ticks+=100001;
 if(!strcmp(engine_mode,"interval-admission-zero"))held_reset_clock_fault=1;
 if(!strcmp(engine_mode,"interval-admission-backward"))held_reset_clock_fault=2;
}
static bool intel_display_test_interval_read(uint32_t off,uint32_t *out) {
 unsigned i=interval_reads++;
#ifdef SRVROS_INTEL_JOINT
 intel_display_test_joint_entry(i+1);
#endif
 assert(i<
#ifdef SRVROS_INTEL_JOINT
 12
#else
 6
#endif
 && off==INTEL_WINDOW_PAGE+(i%3==1?0:4));
 assert(wake_irq && !display_access_allowed() && prepare_stores==5 && held_reset_writes==2 &&
  held_reset_reads==18 && held_reset_boundary==1 && held_boundary==2);
 assert(pa[INTEL_CAPTURE_PAGES+1+13]==0xc0811000);
#ifdef SRVROS_INTEL_REHOLD
 assert(rehold_stores==1);
 for(unsigned e=0;e<5;e++)assert(prepare_controls[e]==3);
#endif
 if(held_reset_at("interval-read",i+1))return false;
 *out=(uint32_t)(window_result.sample[1].entry[0]>>(i%3==1?0:32));
 if(held_reset_at("interval-drift",i+1) || held_reset_at("interval-drift-foreign",i+1))*out^=1;
 if(held_reset_at("interval-drift-foreign",i+1) || held_reset_at("interval-foreign",i+1))wake_request|=2;
 if(held_reset_at("interval-map",i+1))pa[INTEL_CAPTURE_PAGES+1+13]^=4096;
 if(held_reset_at("interval-time",i+1))ticks+=100001;
 return true;
}
#endif

#ifdef SRVROS_INTEL_REHOLD
static void intel_display_test_rehold_source(void *ctx) {
 struct engine_prepare_adapter *p=ctx;
 if(held_reset_at("rehold-admission-owner",1) || held_reset_at("rehold-admission-owner",2)) {
  p->g->begin=ticks-299000;rehold_owner_origin=p->g->begin;
 }
}
static bool intel_display_test_rehold_read(unsigned i,uint32_t off,uint32_t *out) {
 static const uint32_t regs[6]={0x20d0,0x120d0,0x1a0d0,0x1c0d0,0x220d0,0x941c};
 assert(i<6 && off==regs[i] && wake_irq && !display_access_allowed());
 assert(held_reset_writes==2 && held_reset_reads==18 && held_reset_boundary==1 && prepare_stores==5);
 ++rehold_reads;if(held_reset_at("rehold-read",rehold_reads))return false;
 *out=i<5?prepare_controls[i]:0;
 if(held_reset_at("rehold-value",rehold_reads) || held_reset_at("rehold-value-foreign",rehold_reads))*out^=4;
 if(held_reset_at("rehold-value-foreign",rehold_reads) || held_reset_at("rehold-foreign",rehold_reads))wake_request|=2;
 if(held_reset_at("rehold-map",rehold_reads))pa[INTEL_CAPTURE_PAGES+1+(i==5?9:i+3)]^=4096;
 if(held_reset_at("rehold-time",rehold_reads))ticks+=100001;
 return true;
}
static void intel_display_test_rehold_admission(void) {
 if(held_reset_at("rehold-admission-owner",rehold_stores+1)){assert(rehold_owner_origin);ticks=rehold_owner_origin+300001;}
 if(held_reset_at("rehold-admission-time",rehold_stores+1))ticks+=100001;
 if(held_reset_at("rehold-admission-zero",rehold_stores+1))held_reset_clock_fault=1;
 if(held_reset_at("rehold-admission-backward",rehold_stores+1))held_reset_clock_fault=2;
}
static enum intel_gt_write intel_display_test_rehold_transition(uint32_t value,uint32_t *out) {
#ifdef SRVROS_INTEL_JOINT
 assert(intel_display_test_joint_inhibited());
#endif
 ++rehold_stores;assert(rehold_stores<=2 && value==(rehold_stores==1?0x10001u:0x10000u));
 assert(wake_irq && !display_access_allowed() && held_reset_writes==2 && prepare_stores==5);
 if(held_reset_at("rehold-none",rehold_stores))return GT_WRITE_NONE;
 prepare_controls[4]=rehold_stores==1?3:0;*out=prepare_controls[4];
 if(held_reset_at("rehold-ack-float",rehold_stores))*out=UINT32_MAX;
 if(held_reset_at("rehold-store-foreign",rehold_stores))wake_request|=2;
 if(held_reset_at("rehold-store-time",rehold_stores))ticks+=100001;
 return held_reset_at("rehold-uncertain",rehold_stores)?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
#endif
