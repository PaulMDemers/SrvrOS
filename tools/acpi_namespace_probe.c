/* Offline AML evaluation only. No kernel/host hardware access is permitted. */
#define _POSIX_C_SOURCE 200809L
#include <srvros/acpi_ec_resources.h>
#include <uacpi/uacpi.h>
#include <uacpi/acpi.h>
#include <uacpi/context.h>
#include <uacpi/osi.h>
#include <uacpi/namespace.h>
#include <uacpi/tables.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>

#define MAX_TABLES 64
#define MAX_BYTES (4u*1024*1024)
static struct { void *p; size_t size; } mappings[MAX_TABLES+3];
static size_t mapping_count, allocation_bytes, denied, local_writes;
static struct { uintptr_t address; unsigned char *bytes; size_t size; } regions[16];
static size_t region_count, aml_errors;
struct pci_snapshot { uacpi_pci_address address; unsigned char data[256]; };
static struct pci_snapshot pci[16];static size_t pci_count;
static struct acpi_rsdp rsdp;
static void fatal(const char *message) { fprintf(stderr,"probe: %s\n",message);exit(2); }
static void add_mapping(void *p,size_t size) {
    if(mapping_count==MAX_TABLES+3) fatal("too many mappings");
    mappings[mapping_count++]=(typeof(mappings[0])){p,size};
}
uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out) { *out=(uintptr_t)&rsdp;return UACPI_STATUS_OK; }
void *uacpi_kernel_map(uacpi_phys_addr addr,uacpi_size len) {
    for(size_t i=0;i<mapping_count;i++) {
        uintptr_t base=(uintptr_t)mappings[i].p;
        if(addr>=base && addr-base<=mappings[i].size && len<=mappings[i].size-(addr-base)) return (void *)(uintptr_t)addr;
    }
    for(size_t i=0;i<region_count;i++) {
        uintptr_t base=regions[i].address;
        if(addr>=base && addr-base<=regions[i].size && len<=regions[i].size-(addr-base)) return regions[i].bytes+(addr-base);
    }
    fprintf(stderr,"probe: denied mapping 0x%llx length=%zu\n",(unsigned long long)addr,len);denied++;return UACPI_MAP_FAILED;
}
void uacpi_kernel_unmap(void *p,uacpi_size len) { (void)p;(void)len; }
void uacpi_kernel_log(uacpi_log_level level,const char *text) { if(level==UACPI_LOG_ERROR) aml_errors++;fprintf(stderr,"aml[%u]: %s",level,text); }
union allocation_header { max_align_t alignment; size_t charged; };
void *uacpi_kernel_alloc(uacpi_size size) {
    if(!size) size=1;
    if(size>MAX_BYTES-sizeof(union allocation_header)) return NULL;
    size_t charged=size+sizeof(union allocation_header);
    if(charged>MAX_BYTES-allocation_bytes) return NULL;
    union allocation_header *p=malloc(charged);if(!p) return NULL;
    p->charged=charged;allocation_bytes+=charged;return p+1;
}
void uacpi_kernel_free(void *p) {
    if(p) { union allocation_header *h=(union allocation_header *)p-1;allocation_bytes-=h->charged;free(h); }
}
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000+t.tv_nsec; }
static void delay(uint64_t ns) { struct timespec t={ns/1000000000,ns%1000000000};while(nanosleep(&t,&t)); }
void uacpi_kernel_stall(uacpi_u8 us) { delay((uint64_t)us*1000); }
void uacpi_kernel_sleep(uacpi_u64 ms) { if(ms>2000) fatal("sleep exceeds offline budget");delay(ms*1000000); }
uacpi_handle uacpi_kernel_create_semaphore(uacpi_u32 units) { uacpi_u32 *s=uacpi_kernel_alloc(sizeof(*s));if(s) *s=units;return s; }
void uacpi_kernel_free_semaphore(uacpi_handle s) { uacpi_kernel_free(s); }
uacpi_status uacpi_kernel_wait_for_semaphore(uacpi_handle handle,uacpi_u16 timeout) {
    uacpi_u32 *s=handle;if(*s) { --*s;return UACPI_STATUS_OK; }
    if(timeout==0xffff) fatal("blocked single-threaded semaphore");
    if(timeout) delay((uint64_t)timeout*1000000);
    return UACPI_STATUS_TIMEOUT;
}
void uacpi_kernel_signal_semaphore(uacpi_handle handle) { uacpi_u32 *s=handle;if(*s!=UINT32_MAX) ++*s; }
uacpi_thread_id uacpi_kernel_get_thread_id(void) { return (void *)1; }
uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) { return 0; }
void uacpi_kernel_restore_interrupts(uacpi_interrupt_state s) { (void)s; }
uacpi_handle uacpi_kernel_create_spinlock(void) { return (void *)1; }
void uacpi_kernel_free_spinlock(uacpi_handle h) { (void)h; }
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle h) { (void)h;return 0; }
void uacpi_kernel_unlock_spinlock(uacpi_handle h,uacpi_cpu_flags f) { (void)h;(void)f; }
uacpi_handle uacpi_kernel_create_work_item(void) { return (void *)1; }
void uacpi_kernel_free_work_item(uacpi_handle h) { (void)h; }
void uacpi_kernel_schedule_work(uacpi_work_type t,uacpi_handle item,uacpi_work_handler fn,uacpi_handle ctx) {
    (void)t;(void)item;(void)fn;(void)ctx;fatal("unexpected asynchronous work");
}
uacpi_status uacpi_kernel_wait_for_work_completion(void) { return UACPI_STATUS_OK; }
uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *r) { (void)r;denied++;return UACPI_STATUS_DENIED; }
uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq,uacpi_interrupt_handler fn,uacpi_handle ctx,uacpi_handle *out) { (void)irq;(void)fn;(void)ctx;(void)out;denied++;return UACPI_STATUS_DENIED; }
uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler fn,uacpi_handle h) { (void)fn;(void)h;return UACPI_STATUS_DENIED; }
uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address a,uacpi_handle *out) {
    for(size_t i=0;i<pci_count;i++) if(a.segment==pci[i].address.segment && a.bus==pci[i].address.bus && a.device==pci[i].address.device && a.function==pci[i].address.function) { *out=&pci[i];return UACPI_STATUS_OK; }
    fprintf(stderr,"probe: PCI snapshot missing %u:%u:%u.%u\n",a.segment,a.bus,a.device,a.function);
    denied++;return UACPI_STATUS_DENIED;
}
void uacpi_kernel_pci_device_close(uacpi_handle h) { (void)h; }
uacpi_status uacpi_kernel_io_map(uacpi_io_addr base,uacpi_size len,uacpi_handle *out) { (void)out;fprintf(stderr,"probe: SystemIO mapping denied base=%llx len=%zu\n",(unsigned long long)base,len);denied++;return UACPI_STATUS_DENIED; }
void uacpi_kernel_io_unmap(uacpi_handle h) { (void)h; }
#define SNAPSHOT_PCI(bits) \
uacpi_status uacpi_kernel_pci_read##bits(uacpi_handle h,uacpi_size o,uacpi_u##bits *v) { if(o>256-sizeof(*v)) { denied++;return UACPI_STATUS_DENIED; }memcpy(v,((struct pci_snapshot *)h)->data+o,sizeof(*v));return UACPI_STATUS_OK; } \
uacpi_status uacpi_kernel_pci_write##bits(uacpi_handle h,uacpi_size o,uacpi_u##bits v) { if(o>256-sizeof(v)) { denied++;return UACPI_STATUS_DENIED; }memcpy(((struct pci_snapshot *)h)->data+o,&v,sizeof(v));local_writes++;return UACPI_STATUS_OK; }
SNAPSHOT_PCI(8) SNAPSHOT_PCI(16) SNAPSHOT_PCI(32)
#define DENIED_IO(bits) \
uacpi_status uacpi_kernel_io_read##bits(uacpi_handle h,uacpi_size o,uacpi_u##bits *v) { (void)h;(void)o;(void)v;denied++;return UACPI_STATUS_DENIED; } \
uacpi_status uacpi_kernel_io_write##bits(uacpi_handle h,uacpi_size o,uacpi_u##bits v) { (void)h;(void)o;(void)v;denied++;return UACPI_STATUS_DENIED; }
DENIED_IO(8) DENIED_IO(16) DENIED_IO(32)
static void *region_access(void *p,size_t size) {
    uintptr_t address=(uintptr_t)p;
    for(size_t i=0;i<region_count;i++) {
        uintptr_t base=(uintptr_t)regions[i].bytes;
        if(address>=base && address-base<=regions[i].size && size<=regions[i].size-(address-base)) return p;
    }
    fatal("MMIO outside explicit snapshot denied");return NULL;
}
#define SNAPSHOT_MMIO(bits) \
uacpi_u##bits uacpi_kernel_mmio_read##bits(void *p) { uacpi_u##bits v;memcpy(&v,region_access(p,sizeof(v)),sizeof(v));return v; } \
void uacpi_kernel_mmio_write##bits(void *p,uacpi_u##bits v) { memcpy(region_access(p,sizeof(v)),&v,sizeof(v));local_writes++; }
SNAPSHOT_MMIO(8) SNAPSHOT_MMIO(16) SNAPSHOT_MMIO(32) SNAPSHOT_MMIO(64)
static void checksum(void *p,size_t n,size_t at) { unsigned char *b=p;b[at]=0;unsigned sum=0;for(size_t i=0;i<n;i++) sum+=b[i];b[at]=(unsigned char)-sum; }
static void header(struct acpi_sdt_hdr *h,const char *sig,size_t len) {
    memcpy(h->signature,sig,4);h->length=len;h->revision=6;memcpy(h->oemid,"SRVROS",6);memcpy(h->oem_table_id,"OFFLINE ",8);
}
static void check(uacpi_status st,const char *stage) { if(st) { fprintf(stderr,"probe: %s: %s\n",stage,uacpi_status_to_string(st));exit(1); } }
static void display(uacpi_object *object,unsigned depth) {
    if(depth>5) { fputs("<depth limit>",stdout);return; }
    uacpi_u64 integer;uacpi_data_view data;uacpi_object_array list;
    switch(uacpi_object_get_type(object)) {
    case UACPI_OBJECT_INTEGER:check(uacpi_object_get_integer(object,&integer),"integer");printf("0x%llx",(unsigned long long)integer);break;
    case UACPI_OBJECT_STRING:check(uacpi_object_get_string(object,&data),"string");printf("\"%.*s\"",(int)data.length,data.text);break;
    case UACPI_OBJECT_BUFFER:check(uacpi_object_get_buffer(object,&data),"buffer");printf("buffer[%zu]",data.length);for(size_t i=0;i<data.length&&i<256;i++) printf(" %02x",((unsigned char *)data.bytes)[i]);break;
    case UACPI_OBJECT_PACKAGE:check(uacpi_object_get_package(object,&list),"package");putchar('[');for(size_t i=0;i<list.count&&i<64;i++) { if(i) fputs(", ",stdout);display(list.objects[i],depth+1); }putchar(']');break;
    default:printf("<%s>",uacpi_object_type_to_string(uacpi_object_get_type(object)));break;
    }
}
static unsigned char *read_contract_table(const char *path,size_t *length) {
    FILE *f=fopen(path,"rb");if(!f) fatal("cannot open EC contract table");
    if(fseek(f,0,SEEK_END)) fatal("cannot size EC contract table");
    long n=ftell(f);rewind(f);
    if(n<36||n>1024*1024) fatal("invalid EC contract table size");
    unsigned char *p=malloc(n);
    if(!p||fread(p,1,n,f)!=(size_t)n||fclose(f)) fatal("cannot read EC contract table");
    *length=n;return p;
}
static bool audit_contract(const struct acpi_ec_resources *r) {
    const char *names[]={"_HID","_UID","_GPE","_CRS","_GLK"};
    uacpi_object *objects[5]={0};bool glk_absent=false;
    uacpi_namespace_node *node=NULL;uacpi_object_type type=UACPI_OBJECT_UNINITIALIZED;
    bool ok=!uacpi_namespace_node_find(NULL,r->path,&node)&&
        !uacpi_namespace_node_type(node,&type)&&type==UACPI_OBJECT_DEVICE;
    if(!ok) printf("ec-contract: path does not resolve to a device\n");
    for(unsigned i=0;i<5;i++) {
        char path[140];snprintf(path,sizeof(path),"%s.%s",r->path,names[i]);
        uacpi_status st=UACPI_STATUS_OK;
        if(i==4) {
            uacpi_namespace_node *glk_node=NULL;
            st=uacpi_namespace_node_find(NULL,path,&glk_node);
            glk_absent=st==UACPI_STATUS_NOT_FOUND;
        }
        if(!st) st=uacpi_eval_simple(NULL,path,&objects[i]);
        printf("ec-contract: %s = ",path);
        if(glk_absent) printf("<absent>");
        else if(st||!objects[i]) { ok=false;printf("ERROR %s",st?uacpi_status_to_string(st):"no return"); }
        else display(objects[i],0);
        putchar('\n');
    }
    uacpi_u64 hid=0,uid=0,gpe=0,glk=0;uacpi_data_view crs={0},text={0};
    if(ok) {
        bool identity=(!uacpi_object_get_integer(objects[0],&hid)&&hid==0x090cd041)||
            (!uacpi_object_get_string(objects[0],&text)&&text.length==8&&!memcmp(text.text,"PNP0C09",8));
        ok=identity&&!uacpi_object_get_integer(objects[1],&uid)&&
            !uacpi_object_get_integer(objects[2],&gpe)&&!uacpi_object_get_buffer(objects[3],&crs)&&
            acpi_ec_namespace_matches(r,uid,gpe,crs.bytes,crs.length)&&
            (glk_absent||(!uacpi_object_get_integer(objects[4],&glk)&&glk<=1));
    }
    ok=ok&&!aml_errors&&!denied;
    for(unsigned i=0;i<5;i++) if(objects[i]) uacpi_object_unref(objects[i]);
    printf("ec-contract: %s path=%s data=%x command=%x sci=%u gpe=%x status=%x enable=%x mask=%x global_lock=%s hardware_access=none\n",
        ok?"MATCH":"REFUSED",r->path,r->data,r->command,r->sci,r->gpe,r->gpe_status,r->gpe_enable,r->gpe_mask,
        !ok?"unresolved":glk_absent?"absent":glk?"required":"not-requested");
    return ok;
}
int main(int argc,char **argv) {
    /* The independent wall clock also bounds AML outside While loops. */
    alarm(15);setvbuf(stdout,NULL,_IOLBF,0);
    int separator=0,darwin=0;
    bool contract=false;struct acpi_ec_resources ec_contract={0};
    void *tables[MAX_TABLES];size_t table_count=0;
    for(int a=1;a<argc;a++) {
        if(!strcmp(argv[a],"--")) { separator=a+1;break; }
        if(!strcmp(argv[a],"--darwin")) { darwin=1;continue; }
        if(!strcmp(argv[a],"--ec-contract")) {
            if(contract||a+2>=argc) fatal("expected one --ec-contract ECDT.dat FACP.dat");
            size_t en,fn;unsigned char *ec=read_contract_table(argv[++a],&en);
            unsigned char *fa=read_contract_table(argv[++a],&fn);
            enum acpi_ec_resource_result result=acpi_ec_resources_parse(&ec_contract,ec,en,fa,fn);
            free(ec);free(fa);
            if(result) { fprintf(stderr,"probe: EC contract table refusal=%u\n",result);return 1; }
            contract=true;continue;
        }
        if(!strcmp(argv[a],"--pci")) {
            unsigned seg,bus,dev,fn;int n=0;
            if(++a==argc||pci_count==16||sscanf(argv[a],"%x:%x:%x.%x:%n",&seg,&bus,&dev,&fn,&n)!=4||!n||seg>65535||bus>255||dev>31||fn>7) fatal("expected hex SEG:BUS:DEV.FN:FILE PCI snapshot");
            FILE *file=fopen(argv[a]+n,"rb");if(!file) fatal("cannot open PCI snapshot");
            if(fread(pci[pci_count].data,1,256,file)!=256||fgetc(file)!=EOF||fclose(file)) fatal("expected 256-byte PCI snapshot");
            pci[pci_count++].address=(uacpi_pci_address){.segment=seg,.bus=bus,.device=dev,.function=fn};continue;
        }
        if(!strcmp(argv[a],"--region")) {
            if(++a==argc||region_count==16) fatal("expected ADDRESS:FILE region snapshot");
            char *end=NULL;unsigned long long address=strtoull(argv[a],&end,0);
            if(end==argv[a]||*end!=':'||!address) fatal("invalid region address");
            FILE *file=fopen(end+1,"rb");if(!file) fatal("cannot open region snapshot");
            if(fseek(file,0,SEEK_END)) fatal("cannot size region");
            long size=ftell(file);rewind(file);if(size<=0||size>65536||address>UINTPTR_MAX-(size_t)size) fatal("invalid region size");
            unsigned char *bytes=malloc(size);if(!bytes||fread(bytes,1,size,file)!=(size_t)size||fclose(file)) fatal("cannot read region snapshot");
            for(size_t i=0;i<region_count;i++) if(address<regions[i].address+regions[i].size && regions[i].address<address+(size_t)size) fatal("overlapping regions");
            regions[region_count++]=(typeof(regions[0])){address,bytes,size};continue;
        }
        if(table_count==MAX_TABLES) fatal("too many tables");
        FILE *f=fopen(argv[a],"rb");if(!f) fatal("cannot open table");
        if(fseek(f,0,SEEK_END)) fatal("cannot size table");
        long size=ftell(f);rewind(f);
        if(size<36||size>MAX_BYTES) fatal("invalid table size");
        struct acpi_sdt_hdr *h=malloc((size_t)size);if(!h) fatal("allocation failed");
        if(fread(h,1,size,f)!=(size_t)size||fclose(f)) fatal("cannot read table");
        if(h->length!=(size_t)size || memcmp(h->signature,table_count?"SSDT":"DSDT",4)) fatal("expected DSDT then SSDTs with exact lengths");
        unsigned sum=0;for(long i=0;i<size;i++) sum+=((unsigned char *)h)[i];if(sum&255) fatal("bad table checksum");
        add_mapping(h,size);tables[table_count++]=h;
    }
    if(!table_count||!separator||separator==argc) fatal("usage: acpi-namespace [--darwin] [--ec-contract ECDT.dat FACP.dat] DSDT.dat [SSDT.dat ...] -- PATH ...");
    struct acpi_fadt fadt={0};header(&fadt.hdr,"FACP",sizeof(fadt));fadt.x_dsdt=(uintptr_t)tables[0];
    checksum(&fadt,sizeof(fadt),9);add_mapping(&fadt,sizeof(fadt));
    size_t xsdt_size=sizeof(struct acpi_sdt_hdr)+8*table_count;
    struct acpi_sdt_hdr *xsdt=calloc(1,xsdt_size);if(!xsdt) fatal("allocation failed");header(xsdt,"XSDT",xsdt_size);
    uint64_t address=(uintptr_t)&fadt;memcpy((char *)xsdt+36,&address,8);
    for(size_t i=1;i<table_count;i++) { address=(uintptr_t)tables[i];memcpy((char *)xsdt+36+8*i,&address,8); }
    checksum(xsdt,xsdt_size,9);add_mapping(xsdt,xsdt_size);
    memcpy(rsdp.signature,"RSD PTR ",8);memcpy(rsdp.oemid,"SRVROS",6);rsdp.revision=2;rsdp.length=sizeof(rsdp);rsdp.xsdt_addr=(uintptr_t)xsdt;
    checksum(&rsdp,20,8);checksum(&rsdp,sizeof(rsdp),32);add_mapping(&rsdp,sizeof(rsdp));
    check(uacpi_initialize(UACPI_FLAG_NO_ACPI_MODE|UACPI_FLAG_BAD_CSUM_FATAL|UACPI_FLAG_BAD_TBL_SIGNATURE_FATAL|UACPI_FLAG_PROACTIVE_TBL_CSUM),"initialize");
    uacpi_context_set_loop_timeout(1);uacpi_context_set_max_call_stack_depth(32);
    if(darwin) { check(uacpi_bulk_configure_interfaces(UACPI_INTERFACE_ACTION_DISABLE,UACPI_INTERFACE_KIND_VENDOR),"disable vendor interfaces");check(uacpi_install_interface("Darwin",UACPI_INTERFACE_KIND_VENDOR),"Darwin interface"); }
    check(uacpi_namespace_load(),"namespace load");
    unsigned failed=0;
    for(int a=separator;a<argc;a++) {
        uacpi_object *object=NULL;uacpi_status st=uacpi_eval_simple(NULL,argv[a],&object);
        printf("%s = ",argv[a]);if(st) { printf("ERROR %s",uacpi_status_to_string(st));failed++; }else if(object) display(object,0);else printf("<no return>");putchar('\n');
        if(object) uacpi_object_unref(object);
    }
    if(contract&&!audit_contract(&ec_contract)) failed++;
    uacpi_state_reset();failed+=(unsigned)aml_errors;printf("probe: tables=%zu denied=%zu snapshot_writes=%zu remaining_allocated=%zu errors=%u\n",table_count,denied,local_writes,allocation_bytes,failed);
    for(size_t i=0;i<table_count;i++) free(tables[i]);
    free(xsdt);
    for(size_t i=0;i<region_count;i++) free(regions[i].bytes);
    return failed||denied||allocation_bytes?1:0;
}
