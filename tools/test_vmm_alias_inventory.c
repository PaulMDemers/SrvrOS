/* Whole-root census against the actual VMM implementation. */
static void range_lookup_hook(void);
#define SRVROS_VMM_ALIAS_INVENTORY_TEST
#define SRVROS_VMM_RANGE_TEST
#define main old_vmm_main
#include "test_vmm_user_range.c"
#undef main
static unsigned change_at,change_kind;
static void range_lookup_hook(void){if(change_at && table_reads==change_at){if(change_kind==1)test_rflags|=512;else if(change_kind==2)test_cr4|=1ull<<12;else if(change_kind==3)test_cr3^=4096;else if(change_kind==4)address_space_occupancy=1;else if(change_kind==5)address_space_count=1;else test_efer^=1ull<<11;}}
static uint64_t *t[260];static uint64_t root;
static const uint64_t va=0xffff800000000000ull,pa=0xb0000000;
static struct vmm_alias_span spans[2];static struct vmm_alias_ram ram[2];
static struct vmm_alias_inventory result;static struct vmm_alias_detail detail;
static void layout(void){
 for(unsigned i=0;i<260;i++)memset(t[i],0,4096);
 root=kernel_cr3=test_cr3=(uint64_t)t[0];kernel_pml4=t[0];test_cr4=1ull<<5;test_rflags=2;test_maxext=0x80000008u;test_physical_bits=52;test_nx=(1u<<20)|(1u<<26);test_efer=(1ull<<10)|(1ull<<11);
 address_space_count=address_space_occupancy=0;table_reads=change_at=change_kind=0;
 t[0][256]=(uint64_t)t[1]|3;t[1][0]=(uint64_t)t[2]|3;
 for(unsigned n=0;n<6;n++){t[2][n]=(uint64_t)t[3+n]|3;for(unsigned i=0;i<512;i++)t[3+n][i]=(pa+(n*512+i)*4096)|3;}
 spans[0]=(struct vmm_alias_span){va,pa,1356};spans[1]=(struct vmm_alias_span){va+1356*4096ull,pa+1356*4096ull,1356};
 uint64_t lo=UINT64_MAX,hi=0;for(unsigned i=0;i<260;i++){uint64_t p=(uint64_t)t[i];if(p<lo)lo=p;if(p>hi)hi=p;}
 assert(lo>pa+0x1000000);ram[0]=(struct vmm_alias_ram){pa,0x1000000};ram[1]=(struct vmm_alias_ram){lo,hi+4096-lo};
}
static bool check(void){
 uint64_t before[9][512];for(unsigned i=0;i<9;i++)memcpy(before[i],t[i],4096);
 memset(&result,0xa5,sizeof(result));struct vmm_alias_inventory old=result;unsigned sw=switches,ret=retire_count;
 bool ok=vmm_diagnostic_unique_kernel_aliases(root,spans,2,52,ram,2,&result,&detail);
 assert(switches==sw && retire_count==ret);for(unsigned i=0;i<9;i++)assert(!memcmp(before[i],t[i],4096));
 if(ok){assert(result.tables<=256 && result.entries==result.tables*512 && result.pages[0]==spans[0].pages && result.pages[1]==spans[1].pages && !detail.reason);}
 else assert(!memcmp(&result,&old,sizeof(old)) && detail.reason);
 return ok;
}
int main(void){
 for(unsigned i=0;i<260;i++){t[i]=aligned_alloc(4096,4096);assert(t[i]);}
 layout();assert(check() && result.tables==9 && result.leaves==3072 && result.matches[0]==1356 && result.matches[1]==1356);
 /* Every foreign leaf, including both endpoints and partial overlap. */
 for(unsigned n=0;n<2712;n++){
  layout();t[0][0]=(uint64_t)t[9]|3;t[9][0]=(uint64_t)t[10]|3;t[10][0]=(uint64_t)t[11]|3;t[11][511]=(pa+n*4096ull)|3;
  assert(!check() && detail.reason==VMM_ALIAS_FOREIGN);
 }
 layout();for(unsigned n=0;n<6;n++)t[2][n]=(pa+n*0x200000ull)|0x1083;
 assert(check() && result.tables==3 && result.matches[0]==3 && result.matches[1]==4);
 layout();t[1][0]=0x80000000ull|0x83;
 /* Large mapping is unique but requests must use its actual virtual offsets. */
 assert(!check() && detail.reason==VMM_ALIAS_FOREIGN);
 for(unsigned i=0;i<2;i++)spans[i].virtual_base=va+spans[i].physical-0x80000000ull;
 assert(check() && result.tables==2 && result.leaves==1 && result.matches[0]==1 && result.matches[1]==1);
 layout();spans[0].pages=1;spans[0].virtual_base=0xfffffffffffff000ull;spans[1].pages=1;spans[1].virtual_base=va+4096;spans[1].physical=pa+4096;
 t[3][0]=0;t[0][511]=(uint64_t)t[9]|3;t[9][511]=(uint64_t)t[10]|3;t[10][511]=(uint64_t)t[11]|3;t[11][511]=pa|3;assert(check());
 layout();t[0][1]=(uint64_t)t[1]|3;assert(!check() && detail.reason==VMM_ALIAS_FOREIGN); /* legal DAG, foreign aliases */
 for(unsigned depth=0;depth<3;depth++){layout();t[depth][0]=(uint64_t)t[depth]|3;assert(!check() && detail.reason==VMM_ALIAS_CYCLE);}
 layout();t[3][1]=0;assert(!check() && detail.reason==VMM_ALIAS_COVERAGE);
 layout();t[2][0]|=1ull<<51;assert(!check() && detail.reason==VMM_ALIAS_RAM);
 layout();t[2][0]|=1ull<<54;assert(!check() && detail.reason==VMM_ALIAS_RESERVED);
 layout();t[2][0]=pa|3;assert(!check() && detail.reason==VMM_ALIAS_TABLE_BACKING);
 layout();t[0][256]|=128;assert(!check() && detail.reason==VMM_ALIAS_RESERVED);
 layout();t[2][0]=pa|0x2083;assert(!check() && detail.reason==VMM_ALIAS_RESERVED);
 layout();spans[1]=spans[0];assert(!check() && detail.reason==VMM_ALIAS_INPUT);
 layout();spans[0].pages=0;assert(!check());layout();spans[0].pages=4097;assert(!check());
 layout();spans[0].virtual_base=0x7ffffffff000ull;assert(!check());
 layout();spans[0].physical=UINT64_MAX-4095;assert(!check());
 layout();ram[0].length=UINT64_MAX;assert(!check());layout();ram[1]=ram[0];assert(!check());
 layout();ram[1].length=1;assert(!check() && detail.reason==VMM_ALIAS_RAM);
 for(unsigned bit=0;bit<4;bit++){layout();test_cr4|=1ull<<(bit==0?12:bit==1?17:bit==2?22:24);assert(!check() && detail.reason==VMM_ALIAS_MODE);}
 layout();test_cr4=0;assert(!check());layout();test_rflags|=512;assert(!check() && detail.reason==VMM_ALIAS_ROOT);
 layout();address_space_count=1;assert(!check());layout();address_space_occupancy=1;assert(!check());
 layout();assert(check());unsigned conversions=table_reads;
 for(unsigned kind=1;kind<=6;kind++)for(unsigned at=2;at<=conversions;at++){
  layout();change_at=at;change_kind=kind;assert(!check());
 }
 layout();for(unsigned i=0;i<247;i++)t[0][i]=(uint64_t)t[12]|3;t[12][0]=0x40000000ull|0x83;
 assert(check() && result.tables==256 && result.entries==131072);t[0][247]=(uint64_t)t[12]|3;assert(!check() && detail.reason==VMM_ALIAS_LIMIT);
 layout();struct vmm_alias_ram extended[256];for(unsigned i=0;i<254;i++)extended[i]=(struct vmm_alias_ram){0x100000+8192ull*i,4096};extended[254]=ram[0];extended[255]=ram[1];
 assert(vmm_diagnostic_unique_kernel_aliases(root,spans,2,52,extended,256,&result,&detail));extended[255].length=0;
 assert(!vmm_diagnostic_unique_kernel_aliases(root,spans,2,52,extended,256,&result,&detail));
 /* Table paths are bounded even when an unrelated huge DAG is acyclic. */
 layout();for(unsigned i=0;i<255;i++)t[0][i]=(uint64_t)t[12]|3;t[12][0]=(uint64_t)t[13]|3;t[13][0]=0x40000000ull|0x83;
 assert(!check() && detail.reason==VMM_ALIAS_LIMIT);
 layout();test_maxext=0x80000007u;assert(!check() && detail.reason==VMM_ALIAS_MODE);
 layout();test_physical_bits=39;assert(!check());layout();test_efer=0;assert(!check());
 layout();test_efer=1ull<<10;t[3][0]|=VMM_PAGE_NO_EXECUTE;assert(!check() && detail.reason==VMM_ALIAS_RESERVED);
 layout();test_nx=1u<<20;t[1][0]=0x80000000ull|0x83;assert(!check() && detail.reason==VMM_ALIAS_RESERVED);
 layout();struct vmm_alias_inventory old={0};assert(!vmm_diagnostic_unique_kernel_aliases(root,spans,0,52,ram,2,&old,&detail));
 assert(!vmm_diagnostic_unique_kernel_aliases(root,spans,2,53,ram,2,&old,&detail));
 assert(!vmm_diagnostic_unique_kernel_aliases(root,spans,2,52,ram,257,&old,&detail));
 assert(!vmm_diagnostic_unique_kernel_aliases(root,spans,2,52,ram,2,0,&detail));
 layout();assert(check());puts("vmm-alias-inventory: PASS both spans, every foreign page,4K/2M/1G, DAG/cycle/bounds/RAM/mode/root drift, untouched failure and no writes");
 for(unsigned i=0;i<260;i++)free(t[i]);
}
