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
int main(int argc,char **argv) {

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
