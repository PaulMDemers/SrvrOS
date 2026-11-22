/* Guard the actual VMM mutators and whole-root walker, not a parallel model. */
static void vmm_reserved_lookup_hook(void);
static void vmm_reserved_switch_hook(void);
static void vmm_unmapped_efer_hook(void);
#define SRVROS_VMM_RESERVED_TEST
#define SRVROS_VMM_UNMAPPED_CONTEXT_TEST
#define SRVROS_VMM_ALIAS_FIXTURE_MAIN guard_alias_regression
#include "test_vmm_alias_inventory.c"
static struct vmm_reserved_guard guard,other;
static struct vmm_physical_span pair[2];
static struct vmm_unmapped_detail why;
static unsigned checks,tags,bad_tag,hook,efer_fault,efer_target;
static bool recursive;
void console_printf(const char *fmt,...) {(void)fmt;assert(0 && "blocked init reached console");}
static void vmm_unmapped_efer_hook(void) {
 if(efer_target && efer_calls==efer_target) {
  if(efer_fault==1)test_rflags|=512;
  if(efer_fault==2)test_cr3^=4096;
 }
}
static bool begin(struct vmm_reserved_guard *g) {
 return vmm_reserved_guard_begin(g,root,pair,2,52,&ram[1],1,&result,&why);
}
static void vmm_reserved_lookup_hook(void) {
 if(hook!=1 || recursive)return;
 recursive=true;struct vmm_unmapped_detail d;struct vmm_alias_inventory inv;
 assert(!vmm_reserved_guard_begin(&other,root,pair,2,52,&ram[1],1,&inv,&d));
 assert(d.walk.reason==VMM_ALIAS_BUSY && !other.object && reserved_mutations);
 recursive=false;++checks;
}
static void vmm_reserved_switch_hook(void) {
 if(hook!=3)return;
 struct vmm_unmapped_detail d;struct vmm_alias_inventory inv;
 assert(!vmm_reserved_guard_begin(&other,root,pair,2,52,&ram[1],1,&inv,&d));
 assert(d.walk.reason==VMM_ALIAS_BUSY && !other.object && reserved_mutations);++checks;
}
bool pmm_frame_has_tag(uint64_t physical,enum pmm_frame_tag tag) {
 if(physical==0x8b811000) {
  assert(tag==PMM_FRAME_RESERVED);
  return bad_tag!=UINT32_MAX;
 }
 assert(tag==PMM_FRAME_RESERVED && physical==pa+(tags%3072)*4096ull);++tags;
 if(hook==2 && !recursive) {
  recursive=true;struct vmm_unmapped_detail d;struct vmm_alias_inventory inv;
  assert(reserved_guard.owner==&guard);
  assert(!vmm_reserved_guard_begin(&other,root,pair,2,52,&ram[1],1,&inv,&d) && d.walk.reason==VMM_ALIAS_BUSY);
  assert(!vmm_reserved_guard_check(&guard,&inv,&d) && d.walk.reason==VMM_ALIAS_BUSY);
  assert(!vmm_reserved_guard_close(&guard,&inv,&d) && d.walk.reason==VMM_ALIAS_BUSY);
  assert(!vmm_map_page(va,pa,3));
  uint64_t new_root=123;assert(!vmm_create_address_space(&new_root) && !new_root);
  recursive=false;++checks;
 }
 return !bad_tag || tags%3072!=bad_tag%3072;
}
static void setup(void) {
 assert(!reserved_guard.owner && !reserved_mutations);
 layout();memset(&guard,0,sizeof(guard));memset(&other,0,sizeof(other));
 for(unsigned n=0;n<6;n++)memset(t[3+n],0,4096);
 pair[0]=(struct vmm_physical_span){pa,1536};pair[1]=(struct vmm_physical_span){pa+1536*4096ull,1536};
 tags=bad_tag=hook=efer_fault=efer_target=efer_calls=0;recursive=false;
}
static void reset_fault(void) {
 assert(reserved_guard.owner==&guard && reserved_guard.state==VMM_GUARD_FAULT);
 /* Simulate a new machine reset; no production force-release API exists. */
 memset(&reserved_guard,0,sizeof(reserved_guard));reserved_mutations=0;
}
static void refused_map(unsigned route,uint64_t physical,uint64_t flags) {
 uint64_t saved[9][512];for(unsigned i=0;i<9;i++)memcpy(saved[i],t[i],4096);
 unsigned read=table_reads;uint64_t catalog=kernel_mapping_count;
 bool ok=route==0?vmm_map_page(va,physical,flags):route==1?vmm_map_page_in_space(root,va,physical,flags):vmm_ensure_page_in_space(root,va,physical,flags);
 assert(!ok && read==table_reads && catalog==kernel_mapping_count && !reserved_mutations);
 for(unsigned i=0;i<9;i++)assert(!memcmp(saved[i],t[i],4096));
 ++checks;
}
static void submission_window(void) {
 const uint64_t aperture=0xb2200000,backing=0x8b811000,reg=0xc0811000;
 uint64_t original[]={0xc0002000,0xc0012000,0xc001c000,0xc001a000,0xc0022000,0xc0014000,0xc006e000,0xc0065000,0xb2201000,0xb2202000,0xb2203000,0xb2204000};
 uint64_t ports[12];
 uint64_t too_many[13]={0};
 memcpy(too_many,original,sizeof(original));
 too_many[12]=0xc0066000;
 setup();
 assert(!vmm_reserved_guard_begin_submission_window(&guard,root,pair,2,52,&ram[1],1,
         aperture,backing,reg,too_many,13,&result,&why) && !reserved_guard.owner);
 memcpy(ports,original,sizeof(ports));
 for(unsigned n=0;n<12;n++){
  setup();
  t[3][7]=ports[n]|3;
  assert(!vmm_reserved_guard_begin_submission_window(&guard,root,pair,2,52,&ram[1],1,
         aperture,backing,reg,ports,12,&result,&why));
  assert(why.walk.reason==VMM_ALIAS_PRESENT && !reserved_guard.owner);
 }
 setup();
 assert(vmm_reserved_guard_begin_submission_window(&guard,root,pair,2,52,&ram[1],1,
        aperture,backing,reg,ports,12,&result,&why));
 memset(ports,0,sizeof(ports)); /* The caller can discard its list. */
 int child;
 for(unsigned i=0;i<12;i++)assert(vmm_reserved_guard_excludes_io_page(&guard,root,original[i]));
 assert(vmm_reserved_guard_excludes_io_page(&guard,root,aperture));
 assert(!vmm_reserved_guard_excludes_io_page(&guard,root,aperture+5*4096));
 assert(!vmm_reserved_guard_excludes_io_page(&guard,root+4096,aperture));
 assert(vmm_reserved_guard_pin_io_page(&guard,root,aperture,&child));
 assert(vmm_reserved_guard_io_range_pinned(&guard,root,aperture,5,&child));
 for(unsigned i=0;i<8;i++)assert(vmm_reserved_guard_io_range_pinned(&guard,root,original[i],1,&child));
 assert(!vmm_reserved_guard_io_range_pinned(&guard,root,aperture,6,&child));
 assert(vmm_reserved_guard_unpin_io_page(&guard,root,aperture,&child));
 for(unsigned n=0;n<12;n++)for(unsigned route=0;route<3;route++)refused_map(route,original[n],3);
 assert(vmm_reserved_guard_check(&guard,&result,&why));
 assert(vmm_map_page_in_space(root,va+4096,original[0]+4096,3));
 assert(vmm_unmap_page_in_space(root,va+4096,NULL));
 assert(vmm_reserved_guard_close(&guard,&result,&why));
 assert(vmm_map_page_in_space(root,va,original[7],3));
 assert(vmm_unmap_page_in_space(root,va,NULL));
 setup();
 assert(vmm_reserved_guard_begin_submission_window(&guard,root,pair,2,52,&ram[1],1,
        aperture,backing,reg,original,12,&result,&why));
 t[3][7]=original[7]|3; /* Simulate a bypass outside the guarded mutators. */
 assert(!vmm_reserved_guard_close(&guard,&result,&why));
 assert(why.walk.reason==VMM_ALIAS_PRESENT);
 reset_fault();
}
static void command_aperture(void) {
 const uint64_t first=0xb2201000, ports[]={first+8192,first+4096,first+12288};
 int borrower;
 setup();
 assert(vmm_reserved_guard_begin_submission_window(&guard,root,pair,2,52,&ram[1],1,
         first,0x8b811000,0xc0811000,ports,3,&result,&why));
 const uint64_t pipe=0xc0071000;
 assert(!vmm_reserved_guard_exclude_io_range(&guard,root+4096,pipe,2));
 assert(!vmm_reserved_guard_exclude_io_range(&guard,root,pipe,10));
 assert(!vmm_reserved_guard_exclude_io_range(&guard,root,first,2));
 t[3][7]=(pipe+4096)|3;
 assert(!vmm_reserved_guard_exclude_io_range(&guard,root,pipe,2));
 assert(!vmm_reserved_guard_excludes_io_page(&guard,root,pipe));
 assert(!vmm_reserved_guard_excludes_io_page(&guard,root,pipe+4096));
 t[3][7]=0;
 assert(vmm_reserved_guard_exclude_io_range(&guard,root,pipe,2));
 for(unsigned route=0;route<3;route++)for(unsigned i=0;i<2;i++)refused_map(route,pipe+4096*i,3);
 assert(!vmm_reserved_guard_io_range_pinned(&guard,root,first,4,&borrower));
 assert(vmm_reserved_guard_pin_io_page(&guard,root,first,&borrower));
 assert(!vmm_reserved_guard_exclude_io_range(&guard,root,pipe+8192,1));
 assert(vmm_reserved_guard_io_range_pinned(&guard,root,pipe,2,&borrower));
 assert(vmm_reserved_guard_io_range_pinned(&guard,root,first,4,&borrower));
 assert(vmm_reserved_guard_io_range_pinned(&guard,root,first+4096,3,&borrower));
 assert(!vmm_reserved_guard_io_range_pinned(&guard,root,first,5,&borrower));
 assert(!vmm_reserved_guard_io_range_pinned(&guard,root,first,0,&borrower));
 assert(!vmm_reserved_guard_io_range_pinned(&guard,root,first,4,&other));
 assert(vmm_reserved_guard_unpin_io_page(&guard,root,first,&borrower));
 assert(!vmm_reserved_guard_io_range_pinned(&guard,root,first,4,&borrower));
 assert(vmm_reserved_guard_close(&guard,&result,&why));
 assert(vmm_map_page_in_space(root,va,pipe,3));
 assert(vmm_unmap_page_in_space(root,va,NULL));
}
static void register_window(void) {
 const uint64_t aperture=0xb2200000, backing=0x8b811000, reg=0xc0811000;
 const uint64_t flags=VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE|VMM_PAGE_CACHE_DISABLE|VMM_PAGE_NO_EXECUTE;
 int writer,other_writer;
 setup();
 bad_tag=UINT32_MAX;
 assert(!vmm_reserved_guard_begin_register_window(&guard,root,pair,2,52,&ram[1],1,
                                                  aperture,backing,reg,&result,&why));
 assert(why.walk.reason==VMM_ALIAS_PMM_RESERVED);
 setup();
 t[3][7]=backing|3;
 assert(!vmm_reserved_guard_begin_register_window(&guard,root,pair,2,52,&ram[1],1,
                                                  aperture,backing,reg,&result,&why));
 assert(why.walk.reason==VMM_ALIAS_PRESENT);
 setup();
 assert(vmm_reserved_guard_begin_register_window(&guard,root,pair,2,52,&ram[1],1,
                                                 aperture,backing,reg,&result,&why));
 assert(!vmm_reserved_guard_pin_register(&guard,root,aperture,reg,va,&writer));
 assert(vmm_reserved_guard_register_window_held(&guard,root,aperture,backing,reg));
 assert(!vmm_reserved_guard_register_window_held(&guard,root,aperture,backing+4096,reg));
 assert(!vmm_reserved_guard_register_window_held(&guard,root,aperture,backing,reg+4096));
 t[3][0]=reg|flags;
 t[3][8]=reg|flags; /* Even an otherwise identical second leaf is foreign. */
 assert(!vmm_reserved_guard_pin_register(&guard,root,aperture,reg,va,&writer));
 t[3][8]=0;
 const uint64_t bad_flags[]={VMM_PAGE_USER,VMM_PAGE_NO_EXECUTE,VMM_PAGE_WRITABLE,
                             VMM_PAGE_CACHE_DISABLE,VMM_PAGE_WRITE_THROUGH,0x80};
 for(unsigned i=0;i<sizeof(bad_flags)/sizeof(bad_flags[0]);i++) {
  t[3][0]^=bad_flags[i];
  assert(!vmm_reserved_guard_pin_register(&guard,root,aperture,reg,va,&writer));
  t[3][0]^=bad_flags[i];
 }
 assert(!vmm_reserved_guard_pin_register(&guard,root,aperture,reg+4096,va,&writer));
 assert(vmm_reserved_guard_pin_register(&guard,root,aperture,reg,va,&writer));
 assert(!vmm_reserved_guard_pin_register(&guard,root,aperture,reg,va,&other_writer));
 assert(!vmm_reserved_guard_register_pinned(&guard,root,aperture,reg,va,&other_writer));
 assert(!vmm_reserved_guard_close(&guard,&result,&why));
 for(unsigned route=0;route<3;route++)refused_map(route,reg+4096,3);
 assert(!vmm_map_page(va+4096,reg,flags));
 assert(!vmm_map_page_in_space(root,va+4096,backing,flags));
 assert(!vmm_ensure_page_in_space(root,va+4096,aperture,flags));
 assert(!vmm_unmap_page(va,NULL) && !vmm_protect_page_in_space(root,va,3));
 assert(vmm_map_page_in_space(root,va+4096,reg+4096,flags));
 assert(vmm_unmap_page_in_space(root,va+4096,NULL));
 assert(t[3][0]==(reg|flags));
 assert(vmm_reserved_guard_register_pinned(&guard,root,aperture,reg,va,&writer));
 assert(vmm_reserved_guard_unpin_io_page(&guard,root,aperture,&writer));
 assert(vmm_unmap_page(va,NULL));
 assert(vmm_reserved_guard_close(&guard,&result,&why));
}
int main(int argc,char **argv) {
 (void)argv;
 assert(!guard_alias_regression());
 for(unsigned i=0;i<260;i++){t[i]=aligned_alloc(4096,4096);assert(t[i]);}
 setup();assert(begin(&guard) && tags==3072 && result.tables==9 && result.entries==4608);
 if(argc>1){vmm_switch_address_space(root+4096);return 0;}
 for(unsigned page=0;page<3072;page++)for(unsigned route=0;route<3;route++)refused_map(route,pa+page*4096ull,3);
 for(unsigned route=0;route<3;route++) {
  refused_map(route,pa-4096,4096|3); /* flags cannot smuggle physical bits */
  refused_map(route,pa+(1ull<<52),3);
 }
 unsigned reads=table_reads;uint64_t new_root=123,old_switches=switches;
 assert(!vmm_create_address_space(&new_root) && !new_root);
 vmm_init();
 assert(!vmm_prepare_user_kernel_mappings(root+4096));
 vmm_refresh_kernel_mappings(root+4096);vmm_destroy_address_space(root+4096);
 vmm_switch_address_space(0);vmm_switch_address_space(root);
 assert(reads==table_reads && old_switches==switches && !retire_count && !reserved_mutations);
 struct vmm_reserved_guard copy=guard;
 assert(!begin(&other) && !other.object);
 assert(!vmm_reserved_guard_close(&copy,&result,&why) && reserved_guard.owner==&guard);
 assert(!vmm_reserved_guard_check(&copy,&result,&why) && reserved_guard.owner==&guard);
 copy.object=&copy;
 assert(!vmm_reserved_guard_close(&copy,&result,&why) && reserved_guard.owner==&guard);
 /* The native metadata windows can still map/protect/remove outside the pair. */
 assert(vmm_map_page_in_space(root,va,pa-4096,3));
 reads=table_reads;assert(!vmm_protect_page_in_space(root,va,4096|3) && reads==table_reads);
 assert(vmm_protect_page_in_space(root,va,VMM_PAGE_NO_EXECUTE));
 assert((t[3][0]&PAGE_MASK)==pa-4096);
 uint64_t removed=0;assert(vmm_unmap_page_in_space(root,va,&removed) && removed==pa-4096);
 pair[0].physical=0;ram[1].length=1; /* copied immutable inputs */
 assert(vmm_reserved_guard_check(&guard,&result,&why) && tags==6144);
 assert(vmm_reserved_guard_close(&guard,&result,&why) && tags==9216 && !reserved_guard.owner);
 assert(!vmm_reserved_guard_check(&guard,&result,&why));
 assert(!begin(&guard));
 setup();hook=1;assert(vmm_map_page_in_space(root,va,pa-4096,3));assert(checks>9222 && !other.object);hook=0;
 assert(begin(&guard));assert(vmm_reserved_guard_close(&guard,&result,&why));
 setup();hook=3;vmm_switch_address_space(root+4096);hook=0;vmm_switch_address_space(root);
 assert(!other.object && !reserved_guard.owner && !reserved_mutations);
 /* BAR2 provides a second CPU route to the GGTT slot, disjoint from the
  * old stolen pages. Exercise an existing alias, live mutations and close. */
 const uint64_t aperture=0xb2200000;
 setup();t[3][0]=aperture|3;
 assert(!vmm_reserved_guard_begin_io_page(&guard,root,pair,2,52,&ram[1],1,aperture,&result,&why));
 assert(why.walk.reason==VMM_ALIAS_PRESENT && !reserved_guard.owner);
 for(unsigned corrupt=0;corrupt<2;corrupt++) {
  setup();
  assert(vmm_reserved_guard_begin_io_page(&guard,root,pair,2,52,&ram[1],1,aperture,&result,&why));
  assert(vmm_reserved_guard_io_page_held(&guard,root,aperture));
  assert(!vmm_reserved_guard_io_page_held(&other,root,aperture));
  assert(!vmm_reserved_guard_io_page_held(&guard,root+4096,aperture));
  assert(!vmm_reserved_guard_io_page_held(&guard,root,aperture+4096));
  int writer,other_writer;
  assert(vmm_reserved_guard_pin_io_page(&guard,root,aperture,&writer));
  assert(!vmm_reserved_guard_pin_io_page(&guard,root,aperture,&other_writer));
  assert(!vmm_reserved_guard_close(&guard,&result,&why) && why.walk.reason==VMM_ALIAS_BUSY);
  assert(vmm_reserved_guard_io_page_pinned(&guard,root,aperture,&writer));
  assert(!vmm_reserved_guard_unpin_io_page(&guard,root,aperture+4096,&writer));
  assert(!vmm_reserved_guard_unpin_io_page(&guard,root,aperture,&other_writer));
  refused_map(0,aperture,3);
  assert(vmm_reserved_guard_check(&guard,&result,&why));
  assert(vmm_reserved_guard_unpin_io_page(&guard,root,aperture,&writer));
  assert(!vmm_reserved_guard_io_page_pinned(&guard,root,aperture,&writer));
  for(unsigned route=0;route<3;route++)refused_map(route,aperture,3);
  assert(vmm_map_page_in_space(root,va,aperture-4096,3));
  assert(vmm_reserved_guard_check(&guard,&result,&why));
  if(corrupt)t[3][0]=aperture|3;
  assert(vmm_reserved_guard_close(&guard,&result,&why)==!corrupt);
  assert(!vmm_reserved_guard_io_page_held(&guard,root,aperture));
  if(corrupt){assert(why.walk.reason==VMM_ALIAS_PRESENT);reset_fault();}
  else assert(vmm_map_page_in_space(root,va+4096,aperture,3));
 }
 submission_window();
 command_aperture();
 register_window();
 /* Every reserved page is actually queried on admission. */
 for(unsigned page=1;page<=3072;page++) {
  setup();bad_tag=page;memset(&result,0xa5,sizeof(result));struct vmm_alias_inventory saved=result;
  assert(!begin(&guard) && why.walk.reason==VMM_ALIAS_PMM_RESERVED && tags==page);
  assert(!reserved_guard.owner && !memcmp(&saved,&result,sizeof(result)) && guard.object==&guard);++checks;
 }
 for(unsigned phase=0;phase<2;phase++)for(unsigned failure=0;failure<10;failure++) {
  setup();assert(begin(&guard));tags=0;
  if(failure==0)bad_tag=1536;
  if(failure==1)t[3][511]=(pa+3071*4096ull)|3;
  if(failure==2)test_rflags|=512;
  if(failure==3)test_cr4|=1ull<<17;
  if(failure==4)test_efer^=1ull<<11;
  if(failure==5)test_cr3^=4096;
  if(failure==6)address_space_occupancy=1;
  if(failure==7)address_space_count=1;
  if(failure==8)guard.epoch++;
  if(failure==9)guard.object=&other;
  memset(&result,0xa5,sizeof(result));struct vmm_alias_inventory saved=result;
  assert(!(phase?vmm_reserved_guard_close(&guard,&result,&why):vmm_reserved_guard_check(&guard,&result,&why)));
  assert(!memcmp(&saved,&result,sizeof(result)) && reserved_guard.state==VMM_GUARD_FAULT && reserved_guard.owner==&guard);
  refused_map(0,pa-4096,3);assert(!vmm_reserved_guard_close(&guard,&result,&why));
  reset_fault();++checks;
 }
 /* Original mode's final reads are followed by final IF/CR3 guards. */
 for(unsigned which=1;which<=2;which++) {
  setup();assert(begin(&guard));unsigned start=efer_calls;
  assert(vmm_reserved_guard_check(&guard,&result,&why));unsigned reads_efer=efer_calls-start;
  efer_fault=which;efer_target=efer_calls+reads_efer;
  assert(!vmm_reserved_guard_close(&guard,&result,&why) && why.walk.reason==VMM_ALIAS_ROOT);
  reset_fault();++checks;
 }
 setup();hook=2;assert(begin(&guard));assert(vmm_reserved_guard_check(&guard,&result,&why));
 assert(vmm_reserved_guard_close(&guard,&result,&why));hook=0;
 setup();reserved_mutations=UINT32_MAX;assert(!vmm_map_page(va,pa,3) && reserved_mutations==UINT32_MAX);
 assert(!begin(&guard) && !guard.object);reserved_mutations=0;
 setup();test_rflags|=512;assert(!begin(&guard) && !guard.object && !reserved_guard.owner);
 setup();pair[1]=pair[0];assert(!begin(&guard) && !guard.object);
 setup();t[3][0]=pa|3;assert(!begin(&guard) && why.walk.reason==VMM_ALIAS_PRESENT && !reserved_guard.owner);
 for(unsigned bad=0;bad<4;bad++) {
  setup();
  if(bad==0)pair[0].pages=4097;
  if(bad==1)pair[0].physical++;
  if(bad==2)pair[0].physical=UINT64_MAX-4095;
  if(bad==3)ram[1].length=0;
  assert(!begin(&guard) && !reserved_guard.owner);
 }
 setup();uint64_t epoch=reserved_epoch;reserved_epoch=UINT64_MAX;
 assert(!begin(&guard) && !guard.object && !reserved_guard.owner);reserved_epoch=epoch;
 setup();assert(begin(&guard));assert(vmm_reserved_guard_close(&guard,&result,&why));
 /* Without a guard, normal mappings and existing unique-alias contracts remain. */
 assert(vmm_map_page_in_space(root,va,pa,3) && (t[3][0]&PAGE_MASK)==pa);
 for(unsigned i=0;i<260;i++)free(t[i]);
 printf("vmm-reserved-guard: PASS cases=%u actual-mutators=1 in-flight=1 tags=1 retained=1 target-loads=0 hardware-rights=0\n",checks);return 0;
}
