/* Bounded battery/AC diagnostic. Full runtime, linked separately from the
 * snapshot probe and the desktop. CPU0 lifecycle owner and one EC/AML worker. */
#include "battery_platform.h"
#include "work_worker.h"
#include "sci_owner.h"
#include "ec_query_budget.h"
#include <srvros/battery_diagnostic.h>
#include <srvros/acpi.h>
#include <srvros/console.h>
#include <srvros/halt.h>
#include <srvros/heap.h>
#include <srvros/pmm.h>
#include <srvros/semaphore.h>
#include <srvros/sbs_sample.h>
#include <srvros/timer.h>
#include <uacpi/uacpi.h>
#include <uacpi/context.h>
#include <uacpi/namespace.h>
#include <uacpi/osi.h>
#include <uacpi/notify.h>
#include <uacpi/opregion.h>
#include <uacpi/event.h>
#include "../../../../kernel/src/arch/x86_64/io.h"
#include "../../../../kernel/src/arch/x86_64/ioapic.h"
#define HEAP_LIMIT (8u*1024u*1024u)
static struct {
    struct battery_platform platform;
    struct acpi_work_worker worker;
    struct acpi_work_item operation,query;
    struct sci_owner sci;
    uacpi_interrupt_handler irq_fn;
    uacpi_handle irq_context;
    struct ec_transport transport;
    struct sbs_sampler sampler;
    struct ec_query_budget budget;
    uacpi_namespace_node *node,*handler_node;
    uint64_t heap,peak,errors,denied,interrupts,queries,notifications,attached,detached;
    bool engaged,loading,stopping,ready,done,ok,region,notify,gpe,lease,query_scheduled;
    bool ac_present;
    uint64_t ac_sampled_us;
} runtime;
static uint64_t irq_save(void){uint64_t f;__asm__ volatile("pushfq; popq %0; cli":"=r"(f)::"memory");return f;}
static void irq_restore(uint64_t f){if(f&(1ull<<9))__asm__ volatile("sti":::"memory");}
static void event_write(void *,uint16_t,uint8_t);
static uint8_t event_read(void *,uint16_t);
static void mask_sources(void) {
    uint64_t flags=irq_save();
    runtime.stopping=true;
    if(!runtime.engaged){irq_restore(flags);return;}
    outw(runtime.platform.events.resources.pm1_enable,0);
    for(unsigned b=0;b<2;b++)for(unsigned i=0;i<runtime.platform.events.resources.gpe[b].bytes;i++)
        event_write(NULL,runtime.platform.events.resources.gpe[b].enable+i,0);
    if(runtime.sci.phase!=SCI_OWNER_OFF)(void)sci_owner_mask(&runtime.sci);
    irq_restore(flags);
}
static __attribute__((noreturn)) void unsafe(const char *reason) {
    mask_sources();
    console_printf("battery: HALTED %s errors=%u denied=%u heap=%u ec_phase=%u ec_error=%u queries=%u; live state retained\n",
        reason,runtime.errors,runtime.denied,runtime.heap,(uint64_t)runtime.transport.phase,
        (uint64_t)runtime.transport.error,runtime.queries);
    console_sync();__asm__ volatile("cli":::"memory");halt_forever();
}
static bool foreign_port(uint16_t port) {
    for(unsigned i=0;i<BATTERY_FOREIGN_EVENTS;i++) {
        const struct battery_event_hold *h=&runtime.platform.foreign_events[i];
        if(h->mask&&(port==h->status||port==h->enable))return true;
    }
    return false;
}
static uint8_t filter_foreign_event(uint16_t port,bool write,uint8_t value) {
    for(unsigned i=0;i<BATTERY_FOREIGN_EVENTS;i++)
        value=battery_event_hold_filter(&runtime.platform.foreign_events[i],port,write,value);
    return value;
}
static uint8_t event_read(void *ctx,uint16_t port) {
    (void)ctx;
    struct battery_event_ops *io=&runtime.platform.foreign_io;
    if(foreign_port(port)&&io->read)return io->read(io->ctx,port);
    return inb(port);
}
static void event_write(void *ctx,uint16_t port,uint8_t value) {
    (void)ctx;
    struct battery_event_ops *io=&runtime.platform.foreign_io;
    if(foreign_port(port)&&io->write)io->write(io->ctx,port,value);else outb(port,value);
}
static const struct battery_event_ops event_ops={.read=event_read,.write=event_write};
static void check_foreign_event(void) {
    for(unsigned i=0;i<BATTERY_FOREIGN_EVENTS;i++) {
        const struct battery_event_hold *h=&runtime.platform.foreign_events[i];
        if(h->active&&event_read(NULL,h->enable))unsafe("foreign event unexpectedly enabled");
    }
}
static bool contains(uint64_t a,uint64_t n,uint64_t p,uint64_t size){return size&&p>=a&&p-a<=n&&size<=n-(p-a);}
static bool worker_context(void){uint64_t f=irq_save();bool ok=(f&(1ull<<9))&&runtime.worker.identity&&scheduler_current_thread_id()==runtime.worker.identity;irq_restore(f);return ok;}
static void pace(unsigned polls){if(!(polls%16))timer_wait_ticks(1);else timer_delay_us(20);}
uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out){*out=runtime.platform.rsdp;return UACPI_STATUS_OK;}
void *uacpi_kernel_map(uacpi_phys_addr a,uacpi_size n){void *p=runtime.engaged?battery_platform_map(a,n):NULL;if(!p){runtime.denied++;console_printf("battery: denied map address=%x bytes=%u\n",a,(uint64_t)n);}return p?p:UACPI_MAP_FAILED;}
void uacpi_kernel_unmap(void *p,uacpi_size n){(void)p;(void)n;}
void uacpi_kernel_log(uacpi_log_level l,const char *s){if(l==UACPI_LOG_ERROR)runtime.errors++;if(l<=UACPI_LOG_WARN){console_write("battery-aml: ");console_write(s);}}
struct allocation_header { uint64_t charged, padding; };
void *uacpi_kernel_alloc(uacpi_size size) {
    if (!size) size = 1;
    if (size > HEAP_LIMIT - sizeof(struct allocation_header)) return NULL;
    uint64_t charged = size + sizeof(struct allocation_header), flags = irq_save();
    if (charged > HEAP_LIMIT - runtime.heap) { irq_restore(flags); return NULL; }
    struct allocation_header *header = kmalloc(charged);
    if (header) {
        header->charged = charged;
        runtime.heap += charged;
        if (runtime.heap > runtime.peak) runtime.peak = runtime.heap;
    }
    irq_restore(flags);
    return header ? header + 1 : NULL;
}
void uacpi_kernel_free(void *pointer) {
    if (!pointer) return;
    uint64_t flags = irq_save();
    struct allocation_header *header = (struct allocation_header *)pointer - 1;
    runtime.heap -= header->charged;
    kfree(header);
    irq_restore(flags);
}
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    uint64_t us = timer_monotonic_us(); return us > UINT64_MAX / 1000 ? UINT64_MAX : us * 1000;
}
void uacpi_kernel_stall(uacpi_u8 us) { timer_delay_us(us); }
void uacpi_kernel_sleep(uacpi_u64 ms) {
    if (!ms) { scheduler_yield(); return; }
    if (ms == SEMAPHORE_WAIT_FOREVER) unsafe("sleep duration overflows");
    struct kernel_semaphore delay;
    semaphore_init(&delay, 0);
    enum semaphore_result result = semaphore_wait(&delay, ms);
    semaphore_close(&delay);
    if (result != SEMAPHORE_TIMEOUT) unsafe("sleep cannot block");
}
uacpi_handle uacpi_kernel_create_semaphore(uacpi_u32 units) {
    struct kernel_semaphore *semaphore = uacpi_kernel_alloc(sizeof(*semaphore));
    if (semaphore) semaphore_init(semaphore, units);
    return semaphore;
}
void uacpi_kernel_free_semaphore(uacpi_handle handle) {
    if (!handle) return;
    semaphore_close(handle);
    if (!semaphore_drained(handle)) unsafe("semaphore has waiters");
    uacpi_kernel_free(handle);
}
uacpi_status uacpi_kernel_wait_for_semaphore(uacpi_handle handle, uacpi_u16 timeout) {
    enum semaphore_result result = semaphore_wait(handle, timeout == 0xffff ? SEMAPHORE_WAIT_FOREVER : timeout);
    return result == SEMAPHORE_OK ? UACPI_STATUS_OK : result == SEMAPHORE_TIMEOUT ?
        UACPI_STATUS_TIMEOUT : UACPI_STATUS_INTERNAL_ERROR;
}
void uacpi_kernel_signal_semaphore(uacpi_handle handle) {
    if (!semaphore_signal(handle)) unsafe("semaphore signal refused");
}
uacpi_thread_id uacpi_kernel_get_thread_id(void) { return (void *)(uintptr_t)scheduler_current_thread_id(); }
uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) { return irq_save(); }
void uacpi_kernel_restore_interrupts(uacpi_interrupt_state flags) { irq_restore(flags); }
uacpi_handle uacpi_kernel_create_spinlock(void) { return (void *)1; }
void uacpi_kernel_free_spinlock(uacpi_handle handle) { (void)handle; }
uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) { (void)handle; return irq_save(); }
void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) { (void)handle; irq_restore(flags); }
uacpi_handle uacpi_kernel_create_work_item(void) {
    struct acpi_work_item *item = uacpi_kernel_alloc(sizeof(*item));
    if (item) *item = (struct acpi_work_item){0};
    return item;
}
void uacpi_kernel_free_work_item(uacpi_handle handle) {
    if (handle && ((struct acpi_work_item *)handle)->pending) unsafe("free pending work");
    uacpi_kernel_free(handle);
}
void uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_handle item,
    uacpi_work_handler handler, uacpi_handle context) {
    if (!acpi_worker_enqueue(&runtime.worker, item, type, handler, context)) unsafe("work admission refused");
}
uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    /* CPU0 IRQ callbacks cannot sleep: an ordinary-thread IRQ barrier excludes
     * all in-flight callbacks. Teardown masks sources before reaching here. */
    uint64_t flags = irq_save();
    if (!(flags & (1ull << 9))) unsafe("drain with interrupts disabled");
    irq_restore(flags);
    if (runtime.loading && runtime.worker.identity == scheduler_current_thread_id())
        unsafe("namespace load reset requires lifecycle owner");
    if (!acpi_worker_drain(&runtime.worker, 5000)) unsafe("work did not drain");
    return UACPI_STATUS_OK;
}
static bool irq_bridge(void *context) {
    (void)context; runtime.interrupts++;
    check_foreign_event();
    return runtime.irq_fn && runtime.irq_fn(runtime.irq_context) == UACPI_INTERRUPT_HANDLED;
}
static bool sci_read(void *ctx, uint32_t gsi, uint32_t *low, uint32_t *high) {
    (void)ctx; return ioapic_read_gsi(gsi, low, high);
}
static bool sci_prepare(void *ctx, uint32_t gsi, uint8_t vector, bool low, bool level) {
    (void)ctx; return ioapic_prepare_gsi(gsi, vector, low, level);
}
static bool sci_mask(void *ctx, uint32_t gsi, bool masked) {
    (void)ctx; return ioapic_mask_gsi(gsi, masked);
}
static bool sci_restore(void *ctx, uint32_t gsi, uint32_t low, uint32_t high) {
    (void)ctx; return ioapic_restore_masked_gsi(gsi, low, high);
}
static uint64_t sci_register(void *ctx, uint8_t line, irq_handler_fn fn, void *context) {
    (void)ctx;
    return irq_handler_register(line, fn, context);
}
static bool sci_remove(void *ctx, uint64_t token) { (void)ctx; return irq_handler_remove(token); }
uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq,uacpi_interrupt_handler fn,uacpi_handle ctx,uacpi_handle *out) {
    struct sci_route route;
    if(irq!=runtime.platform.ec.sci||!fn||!sci_route_resolve(&route,irq,acpi_irq_override(irq)))return UACPI_STATUS_DENIED;
    struct sci_owner_ops ops={.read=sci_read,.prepare=sci_prepare,.mask=sci_mask,.restore=sci_restore,.register_irq=sci_register,.remove_irq=sci_remove};
    runtime.irq_fn=fn;runtime.irq_context=ctx;
    if(!sci_owner_prepare(&runtime.sci,ops,route,irq_bridge,NULL)||!sci_owner_enable(&runtime.sci)) {
        if(!sci_owner_release(&runtime.sci))unsafe("SCI install rollback");return UACPI_STATUS_DENIED;
    }
    *out=&runtime;return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler fn,uacpi_handle h) {
    if(h!=&runtime||fn!=runtime.irq_fn||!sci_owner_release(&runtime.sci))unsafe("SCI retire");
    runtime.irq_fn=NULL;return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *r){(void)r;runtime.denied++;return UACPI_STATUS_DENIED;}
struct io_region{uint64_t base;size_t size;};
uacpi_status uacpi_kernel_io_map(uacpi_io_addr a,uacpi_size n,uacpi_handle *out) {
    if(!runtime.engaged||!(acpi_event_io_map(&runtime.platform.events,a,n)||battery_platform_io_map(a,n))){runtime.denied++;return UACPI_STATUS_DENIED;}
    struct io_region *r=uacpi_kernel_alloc(sizeof(*r));if(!r)return UACPI_STATUS_OUT_OF_MEMORY;*r=(struct io_region){a,n};*out=r;return UACPI_STATUS_OK;
}
void uacpi_kernel_io_unmap(uacpi_handle h){uacpi_kernel_free(h);}
static uacpi_status port_io(struct io_region *r,size_t o,unsigned bits,bool write,uint32_t *v) {
    if(!runtime.engaged||!r||!contains(0,r->size,o,bits/8))goto denied;
    uint64_t a=r->base+o;
    if(write&&a==runtime.platform.ec.pm1_control&&bits==16&&runtime.worker.identity==scheduler_current_thread_id()) {
        uint64_t flags=irq_save();uint16_t before=inw(a);
        bool ok=acpi_event_io_global_release(&runtime.platform.events,a,bits,before,*v);
        if(ok)outw(a,*v);irq_restore(flags);if(ok)return UACPI_STATUS_OK;goto denied;
    }
    if(write?acpi_event_io_write(&runtime.platform.events,a,bits,*v):acpi_event_io_read(&runtime.platform.events,a,bits)) {
        if(write){if(bits==8)event_write(NULL,a,filter_foreign_event(a,true,*v));else if(bits==16)outw(a,*v);else outl(a,*v);}
        else {
            *v=bits==8?event_read(NULL,a):bits==16?inw(a):inl(a);
            if(bits==8)*v=filter_foreign_event(a,false,*v);
        }
        return UACPI_STATUS_OK;
    }
    if(battery_platform_io(a,bits,write,v))return UACPI_STATUS_OK;
denied:
    runtime.denied++;console_printf("battery: denied IO port=%x width=%u write=%u value=%x\n",r?r->base+o:0,(uint64_t)bits,(uint64_t)write,(uint64_t)*v);return UACPI_STATUS_DENIED;
}
#define PORT(B) \
uacpi_status uacpi_kernel_io_read##B(uacpi_handle h,uacpi_size o,uacpi_u##B *v){uint32_t n=0;uacpi_status s=port_io(h,o,B,false,&n);*v=n;return s;} \
uacpi_status uacpi_kernel_io_write##B(uacpi_handle h,uacpi_size o,uacpi_u##B v){uint32_t n=v;return port_io(h,o,B,true,&n);}
PORT(8) PORT(16) PORT(32)
#define MEMORY(B) \
uacpi_u##B uacpi_kernel_mmio_read##B(void *p){uint64_t v=0;if(!runtime.engaged||!battery_platform_memory(p,B,false,&v)){runtime.denied++;console_printf("battery: denied memory read ptr=%x width=%u\n",(uint64_t)(uintptr_t)p,(uint64_t)B);}return v;} \
void uacpi_kernel_mmio_write##B(void *p,uacpi_u##B v){uint64_t n=v;if(!runtime.engaged||!battery_platform_memory(p,B,true,&n)){runtime.denied++;console_printf("battery: denied memory write ptr=%x width=%u value=%x\n",(uint64_t)(uintptr_t)p,(uint64_t)B,n);}}
MEMORY(8) MEMORY(16) MEMORY(32) MEMORY(64)
uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address a,uacpi_handle *h){if(runtime.engaged&&battery_platform_pci_open(a,h))return UACPI_STATUS_OK;runtime.denied++;return UACPI_STATUS_DENIED;}
void uacpi_kernel_pci_device_close(uacpi_handle h){(void)h;}
#define PCI(B) \
uacpi_status uacpi_kernel_pci_read##B(uacpi_handle h,uacpi_size o,uacpi_u##B *v){uint32_t n=0;if(runtime.engaged&&battery_platform_pci_read(h,o,B,&n)){*v=n;return UACPI_STATUS_OK;}runtime.denied++;return UACPI_STATUS_DENIED;} \
uacpi_status uacpi_kernel_pci_write##B(uacpi_handle h,uacpi_size o,uacpi_u##B v){(void)h;(void)o;(void)v;runtime.denied++;return UACPI_STATUS_DENIED;}
PCI(8) PCI(16) PCI(32)
static bool transfer(bool write,uint8_t a,uint8_t *v,bool query) {
    if(!worker_context()||runtime.lease||runtime.transport.poisoned)return false;
    runtime.lease=true;
    bool ok=query?ec_transport_begin_query(&runtime.transport,timer_monotonic_us(),100000):
        ec_transport_begin(&runtime.transport,write,a,*v,timer_monotonic_us(),100000);
    int result=0;
    for(unsigned polls=1;ok&&!result;polls++){result=ec_transport_poll(&runtime.transport,timer_monotonic_us());if(!result)pace(polls);}
    if(result==1&&!write)*v=runtime.transport.result;
    runtime.lease=false;return ok&&result==1;
}
static uacpi_status region_handler(uacpi_region_op op,uacpi_handle data) {
    if(op==UACPI_REGION_OP_ATTACH){uacpi_region_attach_data *a=data;
        if(a->handler_context!=&runtime||a->generic_info.base>255||!a->generic_info.length||a->generic_info.length>256-a->generic_info.base)goto denied;
        a->out_region_context=&runtime;runtime.attached++;return UACPI_STATUS_OK;}
    if(op==UACPI_REGION_OP_DETACH){runtime.detached++;return UACPI_STATUS_OK;}
    if(op!=UACPI_REGION_OP_READ&&op!=UACPI_REGION_OP_WRITE)goto denied;
    uacpi_region_rw_data *r=data;
    if(r->handler_context!=&runtime||r->region_context!=&runtime||r->byte_width!=1||r->address>255)goto denied;
    uint8_t v=r->value;
    if(!transfer(op==UACPI_REGION_OP_WRITE,r->address,&v,false))goto denied;
    if(op==UACPI_REGION_OP_READ)r->value=v;return UACPI_STATUS_OK;
denied:runtime.denied++;return UACPI_STATUS_DENIED;
}
static uacpi_status notify_handler(uacpi_handle ctx,uacpi_namespace_node *node,uacpi_u64 v) {
    (void)ctx;(void)node;
    if(!worker_context())unsafe("Notify outside worker");
    runtime.notifications++;console_printf("battery: Notify value=%x count=%u\n",v,runtime.notifications);
    return UACPI_STATUS_OK; /* Sampling is periodic; callbacks never nest EC bank IO. */
}
static bool contract(void) {
    if(uacpi_namespace_node_find(NULL,runtime.platform.ec.path,&runtime.node))return false;
    uint64_t hid,uid,gpe,glk=0,bank,sbs;
    if(uacpi_eval_simple_integer(runtime.node,"_HID",&hid)||hid!=0x090cd041||
       uacpi_eval_simple_integer(runtime.node,"_UID",&uid)||uacpi_eval_simple_integer(runtime.node,"_GPE",&gpe))return false;
    uacpi_status gs=uacpi_eval_simple_integer(runtime.node,"_GLK",&glk);
    if((gs&&gs!=UACPI_STATUS_NOT_FOUND)||glk)return false; /* This captured EC does not request a lock. */
    uacpi_object *object=NULL;uacpi_data_view crs;
    if(uacpi_eval_simple(runtime.node,"_CRS",&object)||!object)return false;
    bool ok=!uacpi_object_get_buffer(object,&crs)&&acpi_ec_namespace_matches(&runtime.platform.ec,uid,gpe,crs.bytes,crs.length);
    uacpi_object_unref(object);
    return ok&&!uacpi_eval_simple_integer(runtime.node,"SMB0._EC",&bank)&&bank==0x2010&&
        !uacpi_eval_simple_integer(runtime.node,"SMB0.SBS0._SBS",&sbs)&&sbs==1;
}
static void setup_work(void *ctx) {
    (void)ctx;runtime.ok=false;
    if(!worker_context())unsafe("setup outside worker");
    /* Root handler before namespace load provides the ECDT controller for
     * module-level AML. Exact firmware was checked before granting IO. */
    runtime.handler_node=uacpi_namespace_root();
    if(uacpi_install_address_space_handler(runtime.handler_node,UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER,region_handler,&runtime))goto done;
    runtime.region=true;
    if(uacpi_namespace_load()||!contract())goto done;
    if(uacpi_install_notify_handler(uacpi_namespace_root(),notify_handler,NULL))goto done;
    runtime.notify=true;
    if(uacpi_reg_all_opregions(runtime.handler_node,UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER)||runtime.errors||runtime.denied)goto done;
    if(uacpi_namespace_initialize())goto done;
    runtime.ok=!runtime.errors&&!runtime.denied;
done:runtime.done=true;
}
static void query_work(void *ctx);
static void queue_query(void) {
    if(runtime.stopping||runtime.query_scheduled)return;
    if(!ec_query_budget_begin(&runtime.budget,timer_monotonic_us(),128,2000000))unsafe("query budget begin");
    runtime.query_scheduled=true;
    if(uacpi_suspend_gpe(NULL,runtime.platform.ec.gpe)||uacpi_clear_gpe(NULL,runtime.platform.ec.gpe)||
       !acpi_worker_enqueue(&runtime.worker,&runtime.query,UACPI_WORK_GPE_EXECUTION,query_work,NULL))unsafe("query admission");
}
static bool pending_query(void) {
    uacpi_event_info info=0;
    if(uacpi_gpe_info(NULL,runtime.platform.ec.gpe,&info))unsafe("GPE read");
    return (info&UACPI_EVENT_INFO_HW_STATUS)||(runtime.platform.io.status(runtime.platform.io.context)&32);
}
static uacpi_interrupt_ret gpe_handler(uacpi_handle ctx,uacpi_namespace_node *node,uacpi_u16 idx) {
    (void)ctx;(void)node;
    if(idx!=runtime.platform.ec.gpe)unsafe("wrong EC GPE");
    queue_query();return UACPI_INTERRUPT_HANDLED;
}
static void query_work(void *ctx) {
    (void)ctx;
    if(!worker_context()||runtime.lease)unsafe("query ownership");
    if(runtime.stopping){runtime.query_scheduled=false;return;}
    for(unsigned i=0;i<8;i++) {
        if(!ec_query_budget_take(&runtime.budget,timer_monotonic_us()))unsafe("query limit");
        uint8_t code=0;
        if(!transfer(false,0,&code,true))unsafe("query transport");
        if(code) {
            const char hex[]="0123456789ABCDEF";char method[]="_Q00";method[2]=hex[code>>4];method[3]=hex[code&15];
            console_printf("battery: dispatch EC query=%x\n",(uint64_t)code);
            if(uacpi_execute_simple(runtime.node,method)||runtime.errors||runtime.denied)unsafe("query AML");runtime.queries++;
        }
        if(!ec_query_budget_check(&runtime.budget,timer_monotonic_us()))unsafe("query deadline");
        if(!(runtime.platform.io.status(runtime.platform.io.context)&32))break;
    }
    uint64_t flags=irq_save();
    if(runtime.stopping){runtime.query_scheduled=false;irq_restore(flags);return;}
    if(runtime.platform.io.status(runtime.platform.io.context)&32) {
        if(!acpi_worker_enqueue(&runtime.worker,&runtime.query,UACPI_WORK_GPE_EXECUTION,query_work,NULL))unsafe("query reschedule");
    } else {
        if(!ec_query_budget_finish(&runtime.budget,timer_monotonic_us()))unsafe("query finish");
        runtime.query_scheduled=false;
        if(uacpi_resume_gpe(NULL,runtime.platform.ec.gpe))unsafe("GPE resume");
        if(pending_query())queue_query(); /* Close edge/pending race with IRQs off. */
    }
    irq_restore(flags);
}
static void sample_work(void *ctx) {
    (void)ctx;runtime.ok=false;
    if(!worker_context()||runtime.lease)unsafe("sample ownership");
    uint8_t state=0;
    if(!transfer(false,0x60,&state,false))goto done;
    runtime.ac_present=(state&2)!=0;runtime.ac_sampled_us=timer_monotonic_us();
    runtime.lease=true;
    bool begun=sbs_sample_begin(&runtime.sampler,&runtime.transport,0x20,timer_monotonic_us());int result=0;
    for(unsigned polls=1;begun&&!result;polls++){result=sbs_sample_poll(&runtime.sampler,timer_monotonic_us());if(!result)pace(polls);}
    runtime.lease=false;
    runtime.ok=begun&&result==1&&sbs_sample_fresh(&runtime.sampler,timer_monotonic_us(),1000000)&&
        (runtime.sampler.sample.valid&(SBS_VALID_SOC|SBS_VALID_VOLTAGE|SBS_VALID_CAPACITY))==(SBS_VALID_SOC|SBS_VALID_VOLTAGE|SBS_VALID_CAPACITY);
done:runtime.done=true;
}
static void disconnect_work(void *ctx) {
    (void)ctx;runtime.ok=false;
    if(!worker_context()||runtime.transport.poisoned||runtime.lease)goto done;
    if(runtime.region&&uacpi_uninstall_address_space_handler(runtime.handler_node,UACPI_ADDRESS_SPACE_EMBEDDED_CONTROLLER,region_handler))goto done;
    runtime.region=false;runtime.ok=runtime.attached==runtime.detached;
done:runtime.done=true;
}
static void operation(uacpi_work_handler fn,uint64_t ms,const char *phase) {
    runtime.done=false;runtime.ok=false;
    console_printf("battery: phase=%s\n",phase);
    if(!acpi_worker_enqueue(&runtime.worker,&runtime.operation,UACPI_WORK_NOTIFICATION,fn,NULL))unsafe("operation admission");
    uint64_t deadline=timer_monotonic_us()+ms*1000;
    while(!acpi_worker_drain(&runtime.worker,50)) {
        check_foreign_event();
        if(runtime.budget.active&&!ec_query_budget_check(&runtime.budget,timer_monotonic_us()))unsafe("independent query deadline");
        if(timer_monotonic_us()>=deadline)unsafe("operation deadline");
    }
    check_foreign_event();
    if(!runtime.done||!runtime.ok||runtime.errors||runtime.denied||runtime.transport.poisoned)unsafe(phase);
}
bool battery_diagnostic_run(void *rsdp,struct limine_memmap_response *map) {
    if(runtime.engaged||!timer_reference_ready())return false;
    runtime=(typeof(runtime)){0};
    if(!battery_platform_open(&runtime.platform,rsdp,map)){console_write("battery: REFUSED platform/resource ownership preflight\n");return false;}
    struct sci_route route;uint32_t lo,hi;
    if(!sci_route_resolve(&route,runtime.platform.ec.sci,acpi_irq_override(runtime.platform.ec.sci))||
       !ioapic_read_gsi(route.gsi,&lo,&hi)||!(lo&(1u<<16))||(lo&((1u<<12)|(1u<<14)))) {
        console_write("battery: REFUSED SCI route not free/masked\n");return false;
    }
    runtime.engaged=true;
    for(unsigned i=0;i<BATTERY_FOREIGN_EVENTS;i++) {
        struct battery_event_hold *h=&runtime.platform.foreign_events[i];
        if(!battery_event_hold_begin(h,event_ops))unsafe("foreign event mask");
        if(h->active)console_printf("battery: foreign-event held enable=%x original=%x\n",(uint64_t)h->enable,(uint64_t)h->original);
    }
    if(!acpi_worker_start(&runtime.worker))unsafe("worker start");
    ec_transport_init(&runtime.transport,runtime.platform.io);
    if(uacpi_initialize(UACPI_FLAG_NO_ACPI_MODE|UACPI_FLAG_BAD_CSUM_FATAL|UACPI_FLAG_BAD_TBL_SIGNATURE_FATAL)||runtime.errors||runtime.denied)unsafe("interpreter initialize");
    uacpi_context_set_loop_timeout(1);uacpi_context_set_max_call_stack_depth(32);
    if(uacpi_bulk_configure_interfaces(UACPI_INTERFACE_ACTION_DISABLE,UACPI_INTERFACE_KIND_VENDOR)||
       uacpi_install_interface("Darwin",UACPI_INTERFACE_KIND_VENDOR)||uacpi_enter_acpi_mode())unsafe("ACPI mode/policy");
    check_foreign_event();
    runtime.loading=true;operation(setup_work,5000,"namespace/EC connection");runtime.loading=false;
    /* Match the Linux EC raw edge policy; no firmware AML GPE finalization. */
    unsigned bank=runtime.platform.ec.gpe>=runtime.platform.events.resources.gpe[1].first&&runtime.platform.events.resources.gpe[1].bytes?1:0;
    unsigned byte=(runtime.platform.ec.gpe-runtime.platform.events.resources.gpe[bank].first)/8;
    if(byte>=runtime.platform.events.resources.gpe[bank].bytes)unsafe("GPE policy bounds");
    runtime.platform.events.gpe_enable_mask[bank][byte]=runtime.platform.ec.gpe_mask;
    if(uacpi_install_gpe_handler_raw(NULL,runtime.platform.ec.gpe,UACPI_GPE_TRIGGERING_EDGE,gpe_handler,NULL)||
       uacpi_enable_gpe(NULL,runtime.platform.ec.gpe))unsafe("EC GPE install");
    runtime.gpe=true;runtime.ready=true;
    for(unsigned sample=0;sample<5;sample++) {
        uint64_t flags=irq_save();if(pending_query())queue_query();irq_restore(flags);
        operation(sample_work,25000,"sample");
        const struct sbs_snapshot *s=&runtime.sampler.sample;
        console_printf("battery: sample=%u ac_present=%u soc=%u voltage_uv=%u current_ua=%d capacity=%u full=%u unit=%u temp_mc=%d valid=%x age_us=%u ac_us=%u start_us=%u end_us=%u\n",
            s->generation,(uint64_t)runtime.ac_present,(uint64_t)s->relative_soc,s->voltage_uv,s->current_ua,
            s->remaining_capacity,s->full_capacity,(uint64_t)s->capacity_unit,(int64_t)s->temperature_mc,(uint64_t)s->valid,timer_monotonic_us()-s->completed_us,runtime.ac_sampled_us,s->started_us,s->completed_us);
        for(unsigned tick=0;tick<100;tick++) {
            timer_wait_ticks(1);
            check_foreign_event();
            uint64_t f=irq_save();if(pending_query())queue_query();
            bool expired=runtime.budget.active&&!ec_query_budget_check(&runtime.budget,timer_monotonic_us());irq_restore(f);
            if(expired)unsafe("independent query deadline");
        }
    }
    check_foreign_event();
    mask_sources();
    if(!acpi_worker_drain(&runtime.worker,3000))unsafe("pre-disconnect drain");
    if(uacpi_mask_gpe(NULL,runtime.platform.ec.gpe)||uacpi_uninstall_gpe_handler(NULL,runtime.platform.ec.gpe,gpe_handler))unsafe("GPE uninstall");runtime.gpe=false;
    operation(disconnect_work,3000,"EC disconnect");
    if(runtime.notify&&uacpi_uninstall_notify_handler(uacpi_namespace_root(),notify_handler))unsafe("Notify uninstall");
    if(!acpi_worker_drain(&runtime.worker,3000))unsafe("pre-reset drain");
    /* Reset retires events before its own wait callback: owner already drained. */
    uacpi_state_reset();
    if(!acpi_worker_stop(&runtime.worker,1000)||runtime.heap||runtime.errors||runtime.denied||runtime.sci.phase!=SCI_OWNER_OFF)unsafe("runtime retire");
    if(!(runtime.platform.original_control&1)&&(inw(runtime.platform.ec.pm1_control)&1)) {
        outb(runtime.platform.ec.smi_command,runtime.platform.ec.acpi_disable);
        uint64_t end=timer_monotonic_us()+1000000;
        while((inw(runtime.platform.ec.pm1_control)&1)&&timer_monotonic_us()<end)timer_delay_us(100);
        if(inw(runtime.platform.ec.pm1_control)&1)unsafe("ACPI mode restore");
    }
    for(unsigned i=0;i<BATTERY_FOREIGN_EVENTS;i++) {
        struct battery_event_hold *h=&runtime.platform.foreign_events[i];
        if(!battery_event_hold_restore(h,event_ops))unsafe("foreign event restore");
        if(h->mask)console_printf("battery: foreign-event restored enable=%x original=%x protected_writes=%u\n",
            (uint64_t)h->enable,(uint64_t)h->original,h->protected_writes);
    }
    console_printf("battery: PASS samples=5 notifications=%u queries=%u attached=%u detached=%u heap=0 errors=0 denied=0; physical accuracy NOT_TESTED\n",
        runtime.notifications,runtime.queries,runtime.attached,runtime.detached);
    runtime.engaged=false;return true;
}
bool aml_probe_run(void *rsdp,struct limine_memmap_response *map){(void)rsdp;(void)map;console_write("aml-probe: unavailable in battery diagnostic kernel\n");return false;}
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_memmap_request memmap_request;
void acpi_runtime_test_command(const char *args) {
    static const char command[]="battery";unsigned i=0;
    while(command[i]&&command[i]==args[i])i++;
    if(command[i]||args[i]){console_write("battery diagnostic: acpiruntime battery\n");return;}
    (void)battery_diagnostic_run(rsdp_request.response?rsdp_request.response->address:NULL,memmap_request.response);
}
