#define main old_platform_fixture
#include "test_platform_sample.c"
#undef main
static struct pci_device test_gpu={.vendor_id=0x8086,.device_id=0x0a26,.device=2,.class_code=3};
static struct pci_device test_lpc={.vendor_id=0x8086,.device_id=0x9c43,.device=31,.class_code=6,.subclass=1};
static struct pci_device test_host={.vendor_id=0x8086,.device_id=0x0a04,.class_code=6};
static struct limine_framebuffer fb={.address=(void *)0xffff8000a0000000ull,.width=1366,.height=768,.pitch=5632,.bpp=32,.memory_model=1,.red_mask_size=8,.red_mask_shift=16,.green_mask_size=8,.green_mask_shift=8,.blue_mask_size=8};
static struct limine_framebuffer *fbs[]={&fb};
static struct limine_framebuffer_response frames={.framebuffer_count=1,.framebuffers=fbs};
static struct limine_memmap_entry framebuffer={.base=0xa0000000,.length=4325376,.type=LIMINE_MEMMAP_FRAMEBUFFER};
static struct limine_memmap_entry stolen={.base=0x8d800000,.length=0x2200000,.type=LIMINE_MEMMAP_RESERVED};
static struct limine_memmap_entry ram={.base=0x100000,.length=0x7ff00000,.type=LIMINE_MEMMAP_USABLE};
static struct limine_memmap_entry *entries[]={&framebuffer,&stolen,&ram};
static struct limine_memmap_response memory={.entry_count=3,.entries=entries};
static void setup_adapter(void) {
 memset(pci_config,0,sizeof(pci_config));memset(pch_config,0,sizeof(pch_config));memset(hsw_host_config,0,sizeof(hsw_host_config));
 gpu=&test_gpu;pch_device=&test_lpc;hsw_host=&test_host;gpu_base=0xb0000000;
 pci_config[0]=0x0a268086;pci_config[1]=0x00100007;pci_config[2]=0x03000009;pci_config[4]=0xb0000004;pci_config[6]=0xa000000c;pci_config[0x2c/4]=0x011a106b;
 pci_config[0x34/4]=0x40;pci_config[0x40/4]=0x00010001;pci_config[0x50/4]=0x209;pci_config[0x5c/4]=0x8da00001;
 pch_config[0]=0x9c438086;pch_config[2]=0x06010000;
 hsw_host_config[0]=0x0a048086;hsw_host_config[0x50/4]=0x209;hsw_host_config[0xb0/4]=0x8da00001;hsw_host_config[0xb4/4]=0x8d800001;hsw_host_config[0xbc/4]=0x8fa00001;
 framebuffer_request.response=&frames;memmap_request.response=&memory;frames.framebuffer_count=1;
 hsw_linux_fences=hsw_cpu_control=false;hsw_tile_reads=hsw_tile_change=hsw_upper_change=hsw_upper_ones=0;hsw_tile_value=8;
 hsw_firmware=true;mapped=true;resource_mapped=true;backing_mode=1;mapping_hole=mapping_disjoint=backing_change=backing_bad=0;
 const uint32_t v[]={0x98004400,5632,0,0,0,(1365u<<16)|767,0,0x98004400,0};memcpy(backing_values,v,sizeof(v));
 hsw_window_live=hsw_window_poison=hsw_vmm_live=false;hsw_vmm_maps=hsw_vmm_unmaps=hsw_vmm_map_fail=hsw_vmm_unmap_fail=0;hsw_test_space=0x1000;
 well_value=0x40000000;well_reads=well_drop_at=0;ram.base=0x100000;ram.length=0x7ff00000;stolen.type=LIMINE_MEMMAP_RESERVED;
}
void console_write(const char *text) {(void)text;}

static const struct pci_device test_display_hda={.vendor_id=0x8086,.device_id=0x0a0c,
 .device=3,.class_code=4,.subclass=3};

static void candidate_setup(void) {
 test_candidate=false;
 setup_adapter();
 hsw_cpu_control=hsw_linux_fences=true;
 test_fence_high=0x00fff02b;hsw_tile_value=0;
 /* PM + PCIe v1 RC endpoint, with Transactions Pending set on IGD. */
 pci_config[0x40/4]=0x00016001;
 pci_config[0x60/4]=0x00910010;
 pci_config[0x68/4]=0x00200000;
 hda_device=&test_display_hda;
 memset(hda_config,0,sizeof(hda_config));
 hda_config[0]=0x0a0c8086;hda_config[1]=0x00100003;hda_config[2]=0x04030000;
 hda_config[0x2c/4]=0x011a106b;hda_config[0x34/4]=0x40;hda_config[0x40/4]=0x00010001;
 assert(hsw_capture_bound());
 test_candidate=true;candidate_frames=candidate_ptes=0;
 candidate_frozen=candidate_drift=candidate_late=candidate_bad_route=false;
 memset(&hsw_candidate_result,0,sizeof(hsw_candidate_result));
}

static void access_setup(void) {
 candidate_setup();
 memset(&hsw_access_result,0,sizeof(hsw_access_result));
 access_fbc_controls=access_entries=0;
 access_fbc_drift=access_entry_drift=access_fbc_ones=false;
}

static void access_tests(void) {
 access_setup();
 assert(hsw_candidate_observe_access(true,false) && hsw_vmm_maps==13 && hsw_vmm_unmaps==13);
 assert(hsw_access_result.fbc_rows==2 && hsw_access_result.entries==2 &&
        access_fbc_controls==4 && access_entries==34);
 assert(!hsw_map(NULL,HSW_FBC_PAGE));
 hsw_candidate_window=HSW_CANDIDATE_MMIO;hsw_access_window=true;
 uint32_t word;
 assert(hsw_map(NULL,HSW_FBC_PAGE) && !hsw_read(NULL,HSW_FBC_BASE+4,&word) && hsw_unmap(NULL));
 assert(hsw_map(NULL,HSW_CANDIDATE_MMIO) && !hsw_read(NULL,HSW_CANDIDATE_MMIO+64,&word) && hsw_unmap(NULL));
 hsw_candidate_window=hsw_access_window=false;
 access_setup();access_fbc_drift=true;
 assert(!hsw_candidate_observe_access(true,false) && hsw_candidate_result.status==3 && !hsw_window_live);
 access_setup();access_entry_drift=true;
 assert(!hsw_candidate_observe_access(true,false) && hsw_candidate_result.status==3 && !hsw_window_live);
 access_setup();access_fbc_ones=true;
 assert(!hsw_candidate_observe_access(true,false) && !access_entries && hsw_access_result.fbc_retired==1);
 access_setup();hsw_vmm_unmap_fail=1;
 assert(!hsw_candidate_observe_access(true,false) && hsw_window_poison && !access_entries &&
        hsw_candidate_result.status==5 && !hsw_access_result.fbc_retired);
 access_setup();candidate_late=true;
 assert(!hsw_candidate_observe_access(true,false) && hsw_candidate_result.status==4 &&
        hsw_access_result.fbc_retired==1 && hsw_access_result.entries==1 && !hsw_window_live);
}
static void live_setup(void) {
 access_setup();
 memset(&hsw_live,0,sizeof(hsw_live));
 pci_config[0x60/4]=0x00918810;
 pci_config[0x88/4]=0x01060013;
 pci_config[0x8c/4]=0x100;
 test_smi_count=UINT32_MAX;test_smi_unavailable=false;
}

static void live_audit_tests(void) {
 live_setup();
 assert(hsw_live_snapshot(gpu));
 assert(!hsw_live_match());
 pci_config[1]^=4;
 assert(!hsw_live_snapshot(gpu));
 live_setup();pci_config[0x2c/4]^=1;
 assert(!hsw_live_snapshot(gpu) && !hsw_live.requester_rows);
 live_setup();
 assert(hsw_live_snapshot(gpu) && hsw_live_snapshot(gpu) && !hsw_live_snapshot(gpu));
 hsw_live.slot_probe=true;
 assert(hsw_candidate_observe_access(true,false) && hsw_live_match());
 assert(hsw_live.af[0][0].offset==0x88 && hsw_live.af[1][0].control_status==0x100);
 assert(!hsw_live.af[0][1].offset && hsw_live.smi_begin==UINT32_MAX && hsw_live.smi_end==0);
 hsw_candidate_result.sample[1][HC_IGD_CMD]^=4;
 assert(!hsw_live_match());
 live_setup();hsw_live.slot_probe=true;test_smi_unavailable=true;
 assert(!hsw_candidate_observe_access(true,false) && !hsw_candidate_result.mapped && !hsw_window_live);
}
static void candidate_tests(void) {
 candidate_setup();assert(hsw_candidate_observe_access(false,false) && candidate_ptes==2 && candidate_frames==2 &&
                         hsw_vmm_maps==11 && hsw_vmm_unmaps==11 && !hsw_window_live);
 assert(hsw_candidate_result.sample[0][HC_IGD_DEVSTA]==0x20 &&
        hsw_candidate_result.sample[0][HC_HDA_DEVSTA]==UINT32_MAX);
 assert(!hsw_map(NULL,HSW_CANDIDATE_MMIO));
 candidate_setup();candidate_bad_route=true;assert(!hsw_candidate_observe_access(false,false) && !candidate_ptes && !hsw_window_live);
 candidate_setup();candidate_frozen=true;assert(!hsw_candidate_observe_access(false,false) && hsw_candidate_result.status==3);
 candidate_setup();candidate_drift=true;assert(!hsw_candidate_observe_access(false,false) && hsw_candidate_result.status==3);
 candidate_setup();candidate_late=true;assert(!hsw_candidate_observe_access(false,false) && hsw_candidate_result.status==4 &&
                                              candidate_ptes==1 && !hsw_window_live);
 candidate_setup();hda_config[0x2c/4]^=1;assert(!hsw_candidate_observe_access(false,false) && !candidate_ptes && !hsw_window_live);
 candidate_setup();hsw_vmm_map_fail=1;assert(!hsw_candidate_observe_access(false,false) && hsw_candidate_result.status==5 && !candidate_ptes);
 candidate_setup();hsw_vmm_unmap_fail=1;assert(!hsw_candidate_observe_access(false,false) && hsw_candidate_result.status==5 &&
                                              hsw_window_live && hsw_window_poison);
 candidate_setup();hsw_result.sample[1].fence[31]=1;assert(!hsw_candidate_observe_access(false,false) && hsw_vmm_maps==10);
}
static void boot_slot_tests(void) {
 candidate_setup();
 hsw_candidate_window=HSW_BOOT_SLOT_MMIO;
 uint32_t word;
 assert(!hsw_map(NULL,HSW_CANDIDATE_MMIO));
 assert(hsw_map(NULL,HSW_BOOT_SLOT_MMIO&~4095u));
 assert(!hsw_read(NULL,HSW_BOOT_SLOT_MMIO-4,&word) && !hsw_read(NULL,HSW_BOOT_SLOT_MMIO+4,&word));
 assert(hsw_unmap(NULL));
 hsw_candidate_window=0;
 candidate_setup();assert(!hsw_candidate_observe_access(true,true) && !candidate_ptes);
 candidate_setup();candidate_drift=true;
 assert(!hsw_candidate_observe_access(false,true) && hsw_candidate_result.status==3 && !hsw_window_live);
 candidate_setup();hsw_vmm_unmap_fail=1;
 assert(!hsw_candidate_observe_access(false,true) && hsw_window_poison && hsw_window_live);
}
static void survey_setup(void) {
 candidate_setup();
 memset(&hsw_survey,0,sizeof(hsw_survey));
 survey_reads=survey_route_drop_at=0;
 survey_drift=survey_ones=false;
}
static void survey_tests(void) {
 survey_setup();
 hsw_survey_window=0x208000;hsw_survey_window_end=0x209000;
 uint32_t word;
 assert(!hsw_map(NULL,0x204000) && !hsw_map(NULL,0x210000));
 assert(hsw_map(NULL,0x208000));
 assert(!hsw_read(NULL,0x207ffc,&word) && !hsw_read(NULL,0x209000,&word) && hsw_unmap(NULL));
 hsw_survey_window=0;
 survey_setup();survey_drift=true;
 assert(!hsw_survey_run() && hsw_survey.status==HSW_SURVEY_CHANGED && survey_reads==4097);
 assert(hsw_survey.pass[1].retired==1 && !hsw_window_live);
 survey_setup();survey_ones=true;
 assert(!hsw_survey_run() && hsw_survey.status==HSW_SURVEY_ACCESS && survey_reads==1 && !hsw_window_live);
 survey_setup();candidate_late=true;
 assert(!hsw_survey_run() && hsw_survey.status==HSW_SURVEY_DEADLINE && survey_reads==4096);
 survey_setup();survey_route_drop_at=1024;
 assert(!hsw_survey_run() && hsw_survey.status==HSW_SURVEY_ACCESS && survey_reads==1024);
 assert(hsw_survey.pass[0].retired==1 && !hsw_window_live);
 survey_setup();candidate_frozen=true;
 assert(!hsw_survey_run() && hsw_survey.status==HSW_SURVEY_CHANGED && survey_reads==8192 && !hsw_window_live);
 survey_setup();hsw_vmm_unmap_fail=1;
 assert(!hsw_survey_run() && hsw_survey.status==HSW_SURVEY_RETAINED && survey_reads==1024);
 assert(hsw_window_poison && hsw_window_live && !hsw_survey.pass[0].retired);
}
static void sweep_setup(void) {
 survey_setup();
 memset(&hsw_sweep,0,sizeof(hsw_sweep));
 memset(sweep_visits,0,sizeof(sweep_visits));
 sweep_reads=sweep_stop_at=0;
 test_sweep=true;
 sweep_max_zeros=false;
}
static void sweep_tests(void) {
 sweep_setup();
 uint32_t word;
 hsw_survey_window=0x204000;hsw_survey_window_end=0x205000;
 assert(hsw_map(NULL,0x204000) && !hsw_read(NULL,0x204000,&word) && hsw_unmap(NULL));
 hsw_survey_window=0x210000;hsw_survey_window_end=0x211000;
 assert(hsw_map(NULL,0x210000) && !hsw_read(NULL,0x210004,&word) && hsw_unmap(NULL));
 hsw_survey_window=0;
 assert(!sweep_reads);
 sweep_setup();sweep_stop_at=8191;
 assert(!hsw_sweep_run() && hsw_sweep.status==HSW_SURVEY_ACCESS);
 assert(hsw_sweep.complete==1 && hsw_sweep.attempted==2 && sweep_reads==8191 && !hsw_window_live);
 assert(hsw_sweep.row[1].batch.pass[0].retired==1 && !hsw_sweep.row[1].shortlisted);
}
static void ownership_setup(void) {
 survey_setup();
 test_ownership=true;
 ownership_empty=false;
 hsw_ownership_image_aliases=0;
}
static void ownership_tests(void) {
 ownership_setup();
 uint32_t word;
 hsw_survey_window=0x218000;hsw_survey_window_end=0x218040;
 assert(!hsw_map(NULL,0x204000) && !hsw_map(NULL,0x210000) && !hsw_map(NULL,0x200000));
 assert(hsw_map(NULL,0x218000) && !hsw_read(NULL,0x218040,&word) && hsw_unmap(NULL));
 hsw_survey_window=hsw_survey_window_end=0;
 assert(!survey_reads);
 ownership_setup();survey_drift=true;
 assert(!hsw_ownership_run() && hsw_survey.status==HSW_SURVEY_CHANGED && survey_reads==17);
 assert(hsw_survey.pass[1].retired==1 && !hsw_window_live);
 ownership_setup();ownership_empty=true;
 assert(!hsw_ownership_run() && hsw_survey.status==HSW_SURVEY_NOT_POPULATED && survey_reads==32 && !hsw_window_live);
 ownership_setup();survey_ones=true;
 assert(!hsw_ownership_run() && hsw_survey.status==HSW_SURVEY_ACCESS && survey_reads==1 && !hsw_window_live);
 ownership_setup();candidate_late=true;
 assert(!hsw_ownership_run() && hsw_survey.status==HSW_SURVEY_DEADLINE && survey_reads==16);
 ownership_setup();survey_route_drop_at=16;
 assert(!hsw_ownership_run() && hsw_survey.status==HSW_SURVEY_ACCESS && survey_reads==16 && !hsw_window_live);
 ownership_setup();candidate_frozen=true;
 assert(!hsw_ownership_run() && hsw_survey.status==HSW_SURVEY_CHANGED && survey_reads==32 && !hsw_window_live);
 ownership_setup();hsw_vmm_unmap_fail=1;
 assert(!hsw_ownership_run() && hsw_survey.status==HSW_SURVEY_RETAINED && survey_reads==16);
 assert(hsw_window_poison && hsw_window_live && !hsw_survey.pass[0].retired);
}
static void engine_prereqs_setup(void) {
 live_setup();
 memset(&hsw_survey,0,sizeof(hsw_survey));
 memset(&hsw_engine_prereqs,0,sizeof(hsw_engine_prereqs));
 test_engine_prereqs=true;
 invalid_wake_ack=false;
 wake_ack_reads=thread_status_reads=0;
 pci_config[0x8c/4]=0;
}
static void engine_prereqs_tests(void) {
 engine_prereqs_setup();
 hsw_wake_ack_window=true;
 uint32_t word;
 assert(!hsw_map(NULL,HSW_CANDIDATE_MMIO) && !hsw_map(NULL,HSW_BOOT_SLOT_MMIO&~4095u));
 assert(hsw_map(NULL,HSW_WAKE_ACK_PAGE) && !hsw_read(NULL,HSW_WAKE_ACK-4,&word) &&
        !hsw_read(NULL,HSW_WAKE_ACK+4,&word) && hsw_unmap(NULL) && !wake_ack_reads);
 hsw_wake_ack_window=false;
 engine_prereqs_setup();candidate_frozen=true;
 assert(!hsw_engine_prereqs_run() && hsw_survey.status==HSW_SURVEY_CHANGED && wake_ack_reads==2);
 engine_prereqs_setup();candidate_late=true;
 assert(!hsw_engine_prereqs_run() && hsw_survey.status==HSW_SURVEY_DEADLINE && wake_ack_reads==1);
 engine_prereqs_setup();invalid_wake_ack=true;
 assert(!hsw_engine_prereqs_run() && hsw_survey.status==HSW_SURVEY_ACCESS && wake_ack_reads==1);
 assert(hsw_engine_prereqs.retired==1 && !hsw_window_live && !thread_status_reads);
 engine_prereqs_setup();hsw_vmm_unmap_fail=1;
 assert(!hsw_engine_prereqs_run() && hsw_survey.status==HSW_SURVEY_RETAINED && !hsw_engine_prereqs.retired);
 assert(hsw_window_poison && hsw_window_live && wake_ack_reads==1 && !thread_status_reads);
}
int main(int argc,char **argv) {

 if(argc==2 && !strcmp(argv[1],"engine-prereqs")) {
  engine_prereqs_tests();
  engine_prereqs_setup();assert(hsw_engine_prereqs_run() && wake_ack_reads==2 && thread_status_reads==2);
  assert(hsw_vmm_maps==12 && hsw_vmm_unmaps==12 && candidate_frames==2 && !hsw_window_live && !candidate_ptes);
  hsw_emit=1;hsw_report();hsw_engine_prereqs_report();
  puts("laptop-capture: boot options=srvros.hsw-engine-prereqs=read-only");
  puts("laptop-capture: completed=yes");
  return 0;
 }
 if(argc==2 && !strcmp(argv[1],"ownership")) {
  ownership_tests();
  ownership_setup();assert(hsw_ownership_run() && survey_reads==32 && hsw_ownership_image_aliases==15);
  assert(hsw_vmm_maps==12 && hsw_vmm_unmaps==12 && candidate_frames==2 && !hsw_window_live);
  hsw_emit=1;hsw_report();hsw_ownership_report();
  puts("laptop-capture: boot options=srvros.hsw-ownership=read-only");
  puts("laptop-capture: completed=yes");
  return 0;
 }
 if(argc==2 && (!strcmp(argv[1],"sweep") || !strcmp(argv[1],"sweep-max") || !strcmp(argv[1],"sweep-failed"))) {
  sweep_tests();
  sweep_setup();sweep_max_zeros=!strcmp(argv[1],"sweep-max");
  bool expect_success=strcmp(argv[1],"sweep-failed")!=0;
  if(!expect_success)sweep_stop_at=8191;
  assert(hsw_sweep_run()==expect_success);
  if(expect_success){
   assert(sweep_reads==2*57342 && hsw_sweep.attempted==14 && hsw_sweep.complete==14);
   assert(hsw_vmm_maps==122 && hsw_vmm_unmaps==122 && candidate_frames==28 && !hsw_window_live);
   for(unsigned i=0;i<65536;i++){
    unsigned page=i/1024;
    bool skipped=i<=4096 || i==16385 || page==8 || page==24 || page==40 || page==56;
    assert(sweep_visits[i]==(skipped?0:2));
   }
  }
  hsw_emit=1;hsw_report();hsw_sweep_report();
  puts("laptop-capture: boot options=srvros.hsw-sweep=read-only");
  puts(expect_success?"laptop-capture: completed=yes":"laptop-capture: completed=NO");
  return 0;
 }
 if(argc==2 && !strcmp(argv[1],"survey")) {
  survey_tests();
  survey_setup();assert(hsw_survey_run() && survey_reads==8192 && candidate_frames==2);
  assert(hsw_vmm_maps==18 && hsw_vmm_unmaps==18 && !hsw_window_live);
  hsw_emit=1;hsw_report();hsw_survey_report();
  puts("laptop-capture: boot options=srvros.hsw-survey=read-only");
  puts("laptop-capture: completed=yes");
  return 0;
 }
 if(argc==2 && !strcmp(argv[1],"boot-slot")) {
  boot_slot_tests();
  candidate_setup();assert(hsw_candidate_observe_access(false,true) && candidate_ptes==2);
  assert(hsw_vmm_maps==11 && hsw_vmm_unmaps==11 && !hsw_window_live);
  hsw_emit=1;hsw_report();hsw_candidate_report(true);
  puts("laptop-capture: boot options=srvros.hsw-candidate=read-only srvros.hsw-boot-slot=read-only");
  puts("laptop-capture: completed=yes");
  return 0;
 }
 if(argc==2 && !strcmp(argv[1],"live")) {
  live_audit_tests();
  live_setup();assert(hsw_live_snapshot(gpu));
  test_ticks+=200000; /* Simulated interval reserved for the separate VT-d sampler. */
  assert(hsw_live_snapshot(gpu));
  hsw_live.slot_probe=true;
  assert(hsw_candidate_observe_access(true,false) && hsw_live_match());
  hsw_emit=1;hsw_report();hsw_candidate_report(false);hsw_access_report(true);hsw_live_report(true);
  puts("laptop-capture: completed=yes");
  fprintf(stderr,"hsw-live: PASS PCI brackets, late candidate binding and GT capture\n");
  return 0;
 }
 if(argc==2 && !strcmp(argv[1],"access")) {
  access_tests();
  access_setup();assert(hsw_candidate_observe_access(true,false));
  hsw_emit=1;hsw_report();hsw_candidate_report(false);hsw_access_report(true);puts("laptop-capture: completed=yes");
  fprintf(stderr,"hsw-access: PASS bounded FBC/entry reads, drift, deadline and retirement\n");
  return 0;
 }
 if(argc==2 && !strcmp(argv[1],"candidate")) {
  candidate_tests();
  candidate_setup();assert(hsw_candidate_observe_access(false,false));
  hsw_emit=1;hsw_report();hsw_candidate_report(false);puts("laptop-capture: completed=yes");
  fprintf(stderr,"hsw-candidate: PASS exact reads, wrapped live frame progress, status, drift, and mapping faults\n");
  return 0;
 }
 setup_adapter();unsigned oldmaps=map_calls;assert(hsw_capture_bound());assert(hsw_result.status==1 && hsw_result.count==1056 && hsw_result.sample[0].stolen==1056);
 assert(map_calls==oldmaps && hsw_vmm_maps==8 && hsw_vmm_unmaps==8 && !hsw_window_live && !hsw_vmm_live);
 for(unsigned c=0;c<10;c++) {
  setup_adapter();
  if(c==0)hsw_firmware=false;
  if(c==1)pci_config[0]^=1;
  if(c==2)pci_config[1]&=~2u;
  if(c==3)mapping_hole=3;
  if(c==4)ram.base=0xb0000000;
  if(c==5)hsw_test_space=0x2000;
  if(c==6)hsw_vmm_map_fail=1;
  if(c==7)hsw_vmm_unmap_fail=1;
  if(c==8)frames.framebuffer_count=2;
  if(c==9)backing_bad=5;
  assert(!hsw_capture_bound());
  if(c<5 || c>=8)assert(!hsw_vmm_maps);
  if(c==6 || c==7)assert(hsw_result.retained && hsw_window_poison && hsw_result.status==6);
 }
 setup_adapter();stolen.type=LIMINE_MEMMAP_USABLE;assert(hsw_capture_bound());assert(!hsw_result.sample[0].endpoint.v[HW_STOLEN_RESERVED]);
 setup_adapter();fb.width=384;fb.height=4096;fb.pitch=1536;framebuffer.length=1536*4096;
 backing_values[1]=1536;backing_values[5]=(383u<<16)|4095;
 assert(hsw_capture_bound() && hsw_result.count==1536 && hsw_result.sample[1].count==1536);
 fb.width=1366;fb.height=768;fb.pitch=5632;framebuffer.length=4325376;
 setup_adapter();assert(hsw_capture_bound());assert(!hsw_map(NULL,0x101000));
 setup_adapter();hsw_cpu_control=true;assert(hsw_capture_bound());
 assert(hsw_result.version==2 && hsw_vmm_maps==10 && hsw_vmm_unmaps==10 && hsw_tile_reads==2 && !hsw_window_live);
 assert(hsw_result.sample[0].tilectl==8 && hsw_result.sample[1].tilectl==8 && hsw_result.sample[1].reads==1153);
 setup_adapter();hsw_cpu_control=true;hsw_tile_change=1;assert(!hsw_capture_bound() && hsw_result.status==3 && !hsw_window_live);
 setup_adapter();hsw_cpu_control=true;hsw_tile_value=UINT32_MAX;assert(!hsw_capture_bound() && hsw_result.status==4 && !hsw_window_live);
 setup_adapter();hsw_cpu_control=true;hsw_result.count=1056;hsw_result.offset=0;
 assert(!hsw_map(NULL,0x4000) && !hsw_map(NULL,0x100080));
 assert(hsw_map(NULL,0x101000));uint32_t word;assert(!hsw_read(NULL,0x101004,&word));assert(hsw_read(NULL,0x101000,&word) && word==8);assert(hsw_unmap(NULL));
 setup_adapter();hsw_cpu_control=true;fb.address=(void *)0xffff8000a03ff000ull;framebuffer.base=0xa03ff000;
 backing_values[3]=backing_values[6]=backing_values[8]=0x3ff000;
 assert(!hsw_capture_bound() && hsw_result.status==2 && !hsw_vmm_maps && !hsw_tile_reads);
 fb.address=(void *)0xffff8000a0000000ull;framebuffer.base=0xa0000000;
 setup_adapter();hsw_cpu_control=hsw_linux_fences=true;assert(hsw_capture_bound());
 assert(hsw_result.version==3 && hsw_result.fences==32 && hsw_result.sample[0].reads==1249 && hsw_vmm_maps==10 && hsw_vmm_unmaps==10);
 setup_adapter();hsw_cpu_control=hsw_linux_fences=true;hsw_upper_change=1;assert(!hsw_capture_bound() && hsw_result.status==3 && !hsw_window_live);
 setup_adapter();hsw_cpu_control=hsw_linux_fences=true;hsw_upper_ones=1;assert(!hsw_capture_bound() && hsw_result.status==4 && !hsw_window_live);
 setup_adapter();hsw_result.count=1056;hsw_result.offset=0;assert(hsw_map(NULL,0x100000));uint32_t extra;
 assert(!hsw_read(NULL,0x100080,&extra));hsw_linux_fences=true;assert(hsw_read(NULL,0x100080,&extra));assert(!hsw_read(NULL,0x100100,&extra));assert(hsw_unmap(NULL));
 setup_adapter();hsw_cpu_control=argc>1;hsw_linux_fences=argc>1 && !strcmp(argv[1],"fences32");
 if(argc>2){fb.width=384;fb.height=4096;fb.pitch=1536;framebuffer.length=1536*4096;backing_values[1]=1536;backing_values[5]=(383u<<16)|4095;}
 assert(hsw_capture_bound());hsw_emit=1;hsw_report();puts("laptop-capture: completed=yes");
 fprintf(stderr,"hsw-adapter: PASS exact metadata, kernel-only RO/NX non-global maps, RAM/descriptor/profile refusal and retained failures\n");
}
