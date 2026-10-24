/* Disposable Q35 model. Never linked into the physical diagnostic image. */
#include "battery_platform.h"
#include "q35_fixture.h"
#include <srvros/acpi.h>
#include <srvros/pmm.h>
#include <srvros/console.h>
#include <srvros/timer.h>
#include <srvros/battery_diagnostic.h>
#include <srvros/battery_service.h>
#include <srvros/scheduler.h>
#include <srvros/exfat.h>
#include <srvros/vfs.h>
#include <srvros/block.h>
#include <srvros/usb_storage.h>
#include <srvros/workspace.h>
#include <uacpi/kernel_api.h>
#include <uacpi/tables.h>
#include <uacpi/uacpi.h>
#include <uacpi/namespace.h>
#include "../../../../kernel/src/arch/x86_64/io.h"
static struct battery_platform *platform;
static uint64_t dsdt;
static unsigned reopen_deadline_lifetimes;
static struct {
    uint8_t memory[256], mode,address,output,foreign_status[2],foreign_enable[2];
    uint16_t words[32];
    unsigned step,busy,fault,foreign_reads[2];
    uint64_t generation,commands;
    unsigned ac_reads,query_batch;
    bool deadline_reopen, owner_delayed;
    bool obf,pending,io_started,io_entered,irq_injected,target_armed,alarm_sent,ack_stall,stop_injected;
    uint8_t injected_gpe;
} model;
static uint64_t irq_save(void){uint64_t f;__asm__ volatile("pushfq; popq %0; cli":"=r"(f)::"memory");return f;}
static void irq_restore(uint64_t f){if(f&(1ull<<9))__asm__ volatile("sti":::"memory");}
/* Only this VM platform can mutate a mounted workspace during the test.
 * Native captures retain the narrower fixed scratch-data capability. */
#define FS_BYTES (256u*1024u+37u)
static uint8_t fs_data[FS_BYTES];
static struct { block_write_fn original; unsigned calls; bool failed; } write_fault;
static bool fail_third_write(struct block_device *d,uint64_t off,const void *data,uint64_t n) {
    if(++write_fault.calls==3){write_fault.failed=true;return false;}
    return write_fault.original(d,off,data,n);
}
static bool file_matches(const char *path,const uint8_t *expected,uint64_t length) {
    const struct vfs_node *node=vfs_lookup(path);const uint8_t *data;uint64_t size;
    if(!node||!vfs_read_all(node,&data,&size))return false;
    bool ok=size==length;
    for(uint64_t i=0;ok&&i<size;i++)ok=data[i]==expected[i];
    vfs_release_data(node,data);return ok;
}
static bool filesystem_cycle(uint64_t cycle) {
    const char *dir="/work/srvros-work/fs-proof";
    const char *draft="/work/srvros-work/fs-proof/draft.bin";
    const char *payload="/work/srvros-work/fs-proof/payload.bin";
    const char *empty="/work/srvros-work/fs-proof/empty.bin";
    const char *tmp="/work/srvros-work/fs-proof/transient";
    const char *moved="/work/srvros-work/fs-proof/moved";
    if(!cycle&&(vfs_lookup(dir)||!exfat_create_directory(dir)))return false;
    for(unsigned i=0;i<FS_BYTES;i++)fs_data[i]=(uint8_t)(i*37u+(i>>8)+cycle);
    if(!exfat_write_file(draft,fs_data,FS_BYTES)||!file_matches(draft,fs_data,FS_BYTES)||
       !exfat_write_file(draft,fs_data,37)||!file_matches(draft,fs_data,37)||
       !exfat_write_file(draft,fs_data,FS_BYTES)||
       (cycle&&!exfat_delete_file(payload))||!exfat_rename(draft,payload)||vfs_lookup(draft)||
       !file_matches(payload,fs_data,FS_BYTES)||
       !exfat_write_file(empty,fs_data,4096)||!exfat_write_file(empty,NULL,0)||!file_matches(empty,NULL,0)||
       !exfat_create_directory(tmp)||!exfat_rename(tmp,moved)||vfs_lookup(tmp)||
       !exfat_delete_directory(moved)||vfs_lookup(moved)||!workspace_sync())return false;
    return true;
}
static bool filesystem_failure(void) {
    struct block_device *device=NULL;
    for(unsigned i=0;i<BLOCK_MAX_DEVICES;i++) {
        struct block_device *d=block_device_at(i);
        if(d&&usb_storage_is_device(d)&&block_parent(d)==d&&d->write) {
            if(device)return false;
            device=d;
        }
    }
    if(!device)return false;
    write_fault=(typeof(write_fault)){.original=device->write};
    device->write=fail_third_write;
    bool accepted=exfat_write_file("/work/srvros-work/fs-proof/payload.bin",(const uint8_t *)"replacement",11);
    unsigned calls=write_fault.calls;
    bool blocked=!exfat_write_file("/work/srvros-work/fs-proof/forbidden",fs_data,37)&&
        write_fault.calls==calls&&!exfat_is_writable("/work");
    device->write=write_fault.original;
    bool preserved=file_matches("/work/srvros-work/fs-proof/payload.bin",fs_data,FS_BYTES);
    console_printf("battery-vm: fs-failure injected=%u calls=%u blocked=%u preserved=%u\n",
        (uint64_t)write_fault.failed,(uint64_t)calls,(uint64_t)blocked,(uint64_t)preserved);
    return !accepted&&write_fault.failed&&calls==3&&blocked&&preserved;
}
static void io_hold(void *arg) {
    (void)arg;
    bool filesystem=model.fault==25||model.fault==26;
    uint64_t duration=model.fault==25?60000000:model.fault==21?20000:model.fault>=23?5000000:250000;
    uint64_t f=irq_save(),start=timer_monotonic_us(),reads=0,max_read_us=0;
    bool admitted=scheduler_io_begin(),sampling=false;
    uint64_t commands=model.commands,progress=0,first_generation=0,last_generation=0;
    uint64_t writes=0;bool file_ok=true;
    uint8_t data[4096];
    model.io_entered=true;
    if(admitted) {
        do {
            struct battery_service_status s;
            uint64_t before=timer_monotonic_us();
            battery_service_status(&s);
            uint64_t elapsed=timer_monotonic_us()-before;
            if(elapsed>max_read_us)max_read_us=elapsed;
            if(!reads)first_generation=s.sample.generation;
            last_generation=s.sample.generation;
            reads++;sampling|=(s.flags&BATTERY_CACHE_SAMPLING)!=0;
            if(filesystem && (model.fault==25||!writes)) {
                if(!filesystem_cycle(writes)){file_ok=false;break;}
                writes++;
                if(model.fault==26&&!filesystem_failure()){file_ok=false;break;}
            }
            if(model.fault==24) {
                for(unsigned i=0;i<sizeof(data);i++)data[i]=(uint8_t)(i+writes);
                if(!exfat_write_file("/work/srvros-work/io-proof",data,sizeof(data))||!workspace_sync()) {
                    file_ok=false;break;
                }
                writes++;
            }
            (void)scheduler_io_yield();
        } while(timer_monotonic_us()-start<duration);
        progress=model.commands-commands; /* Capture before releasing exclusion. */
        scheduler_io_end();
    }
    uint64_t elapsed=timer_monotonic_us()-start;
    irq_restore(f);
    console_printf("battery-vm: io-hold admitted=%u sampling=%u requested_us=%u elapsed_us=%u reads=%u max_read_us=%u commands=%u first_gen=%u last_gen=%u\n",
        (uint64_t)admitted,(uint64_t)sampling,duration,elapsed,reads,max_read_us,progress,first_generation,last_generation);
    if(filesystem)console_printf("battery-vm: filesystem mode=%s cycles=%u file_ok=%u bytes=%u\n",
        model.fault==26?"write-failure":"sustained",writes,(uint64_t)file_ok,(uint64_t)FS_BYTES);
    if(model.fault==24)console_printf("battery-vm: usb-io writes=%u file_ok=%u bytes=4096\n",writes,(uint64_t)file_ok);
}
static void interleave_io(void) {
    if(model.io_started||model.generation!=2||(model.fault<21||model.fault>26))return;
    model.io_started=true;
    /* Stop the worker at a transport status read while a different thread
     * holds the real scheduler IO exclusion. No artificial device delay
     * is introduced. Cases 24-26 also write a disposable
     * USB filesystem; other cases only hold the scheduler scope. Once admitted,
     * the worker may resume as soon as its scheduler policy allows it. */
    scheduler_enable_io_services();
    if(!scheduler_spawn("battery-vm-io",io_hold,NULL)) {
        console_write("battery-vm: ERROR io-hold spawn\n");return;
    }
    while(!model.io_entered)scheduler_yield();
}
static bool contains(uint64_t a,uint64_t n,uint64_t p,uint64_t s){return s&&p>=a&&p-a<=n&&s<=n-(p-a);}
static bool equal(const void *a,const char *b,unsigned n){const char *p=a;for(unsigned i=0;i<n;i++)if(p[i]!=b[i])return false;return true;}
static uint8_t status(void *c){(void)c;interleave_io();return (model.fault==2||model.ack_stall?2:0)|(model.obf?1:0)|(model.pending?32:0);}
static uint8_t read_data(void *c){(void)c;model.obf=false;return model.output;}
static void command(void *c,uint8_t v) {
    (void)c;model.commands++;model.mode=v;model.step=0;
    if(v==0x84) {
        /* Observe real query-worker state at the transport boundary. Software
         * injection validates correlation, not physical APIC delivery. */
        if(model.fault==20)__asm__ volatile("int $41":::"memory");
        uint8_t code=model.pending?0x20:0;bool again=model.fault==3;
        if(model.fault>=41 && model.fault<=47 && model.generation==2 && model.pending) {
            code=model.fault==42?0x80:0xff;
            if((model.fault==43 || model.fault==44) && !model.query_batch++){code=0x20;again=true;}
        }
        if(model.fault>=48 && model.generation==2 && model.pending) {
            if(model.fault==49)code=0x80;
            else if(model.fault==50 || model.fault==51)code=0x10;
            else if(model.fault==52 || model.fault==53) {
                static const uint8_t sequence[]={0x40,0x5a,0xce,0xcf,0xd0,0xff};
                unsigned i=model.query_batch++;
                code=i<sizeof(sequence)?sequence[i]:0xff;again=i+1<sizeof(sequence);
            } else if(model.fault==54 || model.fault==55) {
                again=!model.query_batch++;code=again?(model.fault==54?0x20:0x21):0xff;
            } else if(model.fault==48)code=0xff;
        }
        if(model.fault==57 && model.pending)code=0x10;
        if(model.fault==58){code=0;again=false;}
        if((model.fault==59 || model.fault==60 || model.fault==61) && model.generation==2 && model.pending)code=0x10;
        if(model.fault>=62 && model.pending) {
            code=0x10;again=false;model.memory[0x21]=0x80;
            if(((model.fault>=63 && model.fault<=69) || (model.fault==72 && model.generation>=2)) && !model.alarm_sent) {
                model.alarm_sent=true;model.memory[0x21]=0xc0;
                model.memory[0x45]=model.fault==65?0x60:0x16;
                model.memory[0x46]=0x34;model.memory[0x47]=0x12;
                if(model.fault==66)model.memory[0x20]=0x0b;
                if(model.fault==67)model.memory[0x21]|=0x18;
            }
        }
        if(model.fault>=76 && model.fault<=79 && model.pending && model.generation>=2 && !(model.generation%2)) {
            model.memory[0x21]=0xc0;
            model.memory[0x45]=(model.fault==76 && model.generation%4)?0x60:0x16;
            model.memory[0x46]=0x34;model.memory[0x47]=0x12;
        }
        if((model.fault==80 || model.fault==81 || model.fault==83 || model.fault==91 || model.fault==92) && model.pending) {
            unsigned batch=model.fault==80?15:32;
            again=++model.query_batch<batch;
        }
        /* An acknowledged alarm followed by a refused query in one batch has
         * effects: normal-return recovery must reject that receipt. */
        if(model.fault==82 && model.pending) {
            if(!model.query_batch++) {
                code=0x10;again=true;model.memory[0x21]=0xc0;
                model.memory[0x45]=0x16;model.memory[0x46]=0x34;model.memory[0x47]=0x12;
            } else {code=0xff;again=false;}
        }
        if(model.fault>=85 && model.fault<=89 && model.pending) {
            /* Realistic completion batches plus an AC notification at changes. */
            again=++model.query_batch<16;
            if(model.query_batch==16 && (model.generation==4 || model.generation==7))code=0x21;
            if(model.fault==88 && model.generation==4) {code=0xff;again=false;}
            if(model.fault==89 && model.generation==4 && model.query_batch==1) {
                model.memory[0x21]=0xc0;model.memory[0x45]=0x16;
                model.memory[0x46]=0x34;model.memory[0x47]=0x12;
            }
        }
        if(model.fault==73){code=0;again=false;}
        if(model.fault==75 && model.generation==2 && model.pending)code=0xff;
        model.output=code;model.pending=again;
        model.obf=!((model.fault==46 || model.fault==59) && model.generation==2);
    }
}
static void stop_during_alarm(bool acknowledgement) {
    if(model.stop_injected || model.generation<4 || model.fault<77 || model.fault>82 ||
       acknowledgement!=(model.fault>=78))return;
    model.stop_injected=true;
    bool synchronous=battery_service_stop(0);
    struct battery_service_status s;battery_service_status(&s);
    console_printf("battery-vm-smbus-stop: phase=%s generation=%u state=%u synchronous=%u\n",
        acknowledgement?"ack":"read",model.generation,(uint64_t)s.state,(uint64_t)synchronous);
}
static void write_data(void *c,uint8_t v){
    (void)c;
    if(!model.step++) {
        model.address=v;
        if(model.mode==0x80){
            if(v==0x20&&model.busy&&!--model.busy)model.memory[0x20]=0;
            if(v==0x46)stop_during_alarm(false);
            model.output=model.memory[v];
            if(v==0x60&&((model.fault==17&&model.generation==2)||(model.fault==86&&model.generation==4))&&model.ac_reads++)model.output^=2;
            model.obf=!(model.fault==68 && v==0x46);
        }
        return;
    }
    model.memory[model.address]=v;
    if(model.address==0x21) {
        stop_during_alarm(true);
        if(model.fault==69 || (model.fault==79 && model.stop_injected))model.ack_stall=true;
    }
    if(model.address==0x20&&v==9&&model.memory[0x22]==0x16&&model.memory[0x23]<32){
        uint16_t word=model.words[model.memory[0x23]];model.memory[0x24]=word;model.memory[0x25]=word>>8;model.busy=2;
    }
}
static uacpi_table_installation_disposition install(struct acpi_sdt_hdr *h,uacpi_u64 *override){
    if(!equal(h->signature,"DSDT",4))return UACPI_TABLE_INSTALLATION_DISPOSITION_ALLOW;
    if(equal(h->oem_table_id,"BATTEST ",8))return UACPI_TABLE_INSTALLATION_DISPOSITION_DENY;
    *override=dsdt;return UACPI_TABLE_INSTALLATION_DISPOSITION_PHYSICAL_OVERRIDE;
}
/* Two shadow GPE bytes exercise the interpreter's W1C requests and partial
 * lease/restore failures. They are not physical graphics/alarm sources. */
static unsigned foreign_index(uint16_t port){return port==0x626||port==0x62e?1:0;}
static uint8_t foreign_read(void *ctx,uint16_t port) {
    (void)ctx;unsigned i=foreign_index(port);
    if(port>=0x628&&model.fault==10&&i==0&&++model.foreign_reads[i]==4)model.foreign_enable[i]=0x40;
    return port<0x628?model.foreign_status[i]:model.foreign_enable[i];
}
static void foreign_write(void *ctx,uint16_t port,uint8_t value) {
    (void)ctx;unsigned i=foreign_index(port);uint8_t mask=i?4:0x40;
    if(port<0x628) {
        if(value&mask)console_write("battery-vm: ERROR foreign status acknowledged\n");
        model.foreign_status[i]&=~value;
    } else {
        if(model.fault==9&&i==1&&value==0)return;
        if(model.fault==32&&model.generation==3&&i==0&&model.foreign_enable[0]&&value==0)return;
        if(model.fault==8&&i==1&&value==mask)return;
        model.foreign_enable[i]=value;
        if(value==mask)console_printf("battery-vm: foreign restored enable=%x pending=%x\n",(uint64_t)port,(uint64_t)model.foreign_status[i]);
    }
}
static bool model_irq_fault(void) {
    if(model.generation!=3)return false;
    if(model.fault>=36) { model.target_armed=true;return true; }
    if(model.fault>=33) { model.irq_injected=true;model.injected_gpe=2; }
    else model.foreign_enable[0]=0x40;
    return true;
}
static bool model_event(uint16_t port,bool write,uint8_t *value) {
    if(port!=0x620)return false;
    if(write) { model.injected_gpe&=~*value;return false; } /* Also clear real W1C bits. */
    *value=inb(port)|model.injected_gpe;return true;
}
static bool model_query_refuse(void) {
    if(!model.irq_injected)return false;
    /* Deliberately leave a callback spinlock unmatched. The returned-callback
     * boundary must refuse it, even though the CPU0 lock is IRQ masking only. */
    if(model.fault==35)(void)uacpi_kernel_lock_spinlock((void *)1);
    return true;
}
static void target_irq(void) {
    model.irq_injected=true;model.injected_gpe=2;
    __asm__ volatile("int $41":::"memory");
    console_write("battery-vm: ERROR target resumed after fault\n");
}
static void model_owner_irq(void) {
    if(model.fault==91 && model.deadline_reopen && !model.owner_delayed) {
        model.owner_delayed=true;
        console_write("battery-vm: reopen owner delayed after pending drain\n");
        uacpi_kernel_sleep(5100);
        console_write("battery-vm: reopen owner resumed beyond deadline\n");
    }
    if(model.fault==37 && model.generation==3 && model.target_armed && !model.irq_injected)target_irq();
}
static void model_transfer_wait(void) {
    if(model.fault==40 && model.generation==3 && !model.irq_injected) {
        model.irq_injected=true;
        console_write("battery-vm: partial EC transaction held\n");
        uacpi_kernel_sleep(26000);
    }
}
static void model_query_checkpoint(uint8_t code) {
    if((model.fault==45 && code==0xff) || (model.fault==61 && code==0x10)) {
        uacpi_u32 sequence;
        if(uacpi_acquire_global_lock(100,&sequence)==UACPI_STATUS_OK)
            console_write("battery-vm: query Global Lock retained\n");
    }
}
static void model_sample_begin(uint64_t generation) {
    model.generation=generation;model.ac_reads=0;
    if(model.fault==90 && generation==2) {
        console_write("battery-vm: delayed second sample begin ms=4000\n");
        uacpi_kernel_sleep(4000);
        console_write("battery-vm: delayed second sample end\n");
    }
    if(model.fault>=80)model.query_batch=0;
    if(model.fault==38 && generation==3) {
        uacpi_u32 seq;
        if(uacpi_acquire_global_lock(100,&seq)==UACPI_STATUS_OK) {
            console_write("battery-vm: API Global Lock held\n");
            uacpi_kernel_sleep(26000);
            (void)uacpi_release_global_lock(seq);
        }
    }
    if(model.fault==39 && generation==3)(void)uacpi_execute_simple(NULL,"\\HOLD");
    /* Active-service fault. Model 27 exercises an armed parked-workspace reset;
     * model 28 retains a mounted workspace and must block that same reset. */
    if(model.fault==29 && generation==3) {
        /* Deliberately violate the adapter's no-storage contract. Quarantine
         * must refuse a worker which owns I/O, preserving terminal containment. */
        uint64_t flags=irq_save();bool entered=scheduler_io_begin();irq_restore(flags);
        console_printf("battery-vm: worker storage owner=%s\n",entered?"yes":"NO");
        uacpi_kernel_sleep(26000);
        flags=irq_save();if(entered)scheduler_io_end();irq_restore(flags);
    }
    if(model.fault==36 && generation==3) {
        uint64_t until=timer_monotonic_us()+5000000;
        while(!model.target_armed && timer_monotonic_us()<until)uacpi_kernel_sleep(10);
        if(model.target_armed)target_irq();
    }
    if((model.fault==31||model.fault==34) && generation==3) {
        model_irq_fault();__asm__ volatile("int $41":::"memory");
    }
    if((model.fault==27||model.fault==28||model.fault==30||model.fault==32||model.fault==33||model.fault==35||model.fault==37)&&generation==3)uacpi_kernel_sleep(26000);
    /* Deliver a deliberately unclaimed SCI through the real dispatcher. This
     * verifies accounting without inventing hardware status or claiming it. */
    if(model.fault==20)__asm__ volatile("int $41":::"memory");
    /* Fixture 84 leaves time to save each Home screenshot before the next
     * synthetic transition. It changes no native timing or policy. */
    bool discharging=(model.fault==84&&generation>=9&&generation<=24)||(model.fault==13&&generation>=4&&generation<=6)||
        (model.fault>=15&&model.fault!=84&&generation>=3&&generation<=4);
    if(model.fault>=85 && model.fault<=89)discharging=model.fault!=87 && generation>=4 && generation<=((model.fault==86 || model.fault==89)?7:6);
    model.memory[0x60]=discharging?1:3;
    model.words[10]=discharging?(uint16_t)-250:generation>6?250:0;
    model.words[13]=model.fault==16&&generation>=2&&generation<=4?101:86;
    model.pending=true;
}
static void model_mode_entered(void) {
    uint16_t value=model.fault==11?1u<<8:1u<<9;
    outw(0x602,value);
    console_printf("battery-vm: injected post-mode fixed-enable=%x\n",(uint64_t)inw(0x602));
}
static void model_setup_work(void) {
    if(model.fault==91 || model.fault==92) {
        if(model.deadline_reopen) {
            console_write("battery-vm: second setup delay armed\n");
            uacpi_kernel_sleep(model.fault==91?100:6000);
        }
        return;
    }
    if(model.fault==19) {
        /* Armed before engagement, so an I/O scope cannot prevent this modeled
         * deadline from exercising the contained reset path. */
        console_write("battery-vm: unattended setup deadline armed\n");
        uacpi_kernel_sleep(6000);
        return;
    }
    /* Accumulate short executing slices, excluding long gaps when another
     * runnable thread owns the CPU. A reader that HLTs without blocking can
     * double this workload's wall time and exhaust the unchanged setup budget.
     * This models scheduling pressure, not the contents of native firmware. */
    uint64_t start=timer_monotonic_us(),last=start,active=0;
    while(active<3500000) {
        uint64_t now=timer_monotonic_us(),delta=now-last;
        if(delta<=100)active+=delta;
        last=now;
        __asm__ volatile("pause");
    }
    console_printf("battery-vm: setup-work active_us=%u elapsed_us=%u\n",
        active,timer_monotonic_us()-start);
}
bool battery_platform_open(struct battery_platform *p,void *rsdp,struct limine_memmap_response *map){
    (void)map;dsdt=0;platform=NULL;model=(typeof(model)){0};
    const struct acpi_fadt *f=acpi_q35_fixture_fadt();if(!f||!p||!rsdp)return false;
    for(uint64_t i=0;i<acpi_table_count();i++){
        const struct acpi_table_info *t=acpi_table_at(i);const struct acpi_sdt_hdr *h=pmm_phys_to_virt(t->physical_address);
        if(equal(t->signature,"DSDT",4)&&equal(h->oem_table_id,"BATTEST ",8)){if(dsdt)return false;dsdt=t->physical_address;model.fault=h->oem_revision;}
    }
    if(!dsdt||model.fault>92||acpi_event_resources_parse(&p->events.resources,(const uint8_t *)f,f->hdr.length)||inw(0x602))return false;
    for(unsigned port=0x628;port<0x630;port++)if(inb(port))return false;
    if(model.fault==91 || model.fault==92)model.deadline_reopen=++reopen_deadline_lifetimes==2;
    p->ec=(struct acpi_ec_resources){.data=0x62,.command=0x66,.sci=9,.smi_command=0xb2,.pm1_control=0x604,
        .gpe_status=0x620,.gpe_enable=0x628,.gpe=1,.gpe_mask=2,.acpi_enable=f->acpi_enable,.acpi_disable=f->acpi_disable,.path="\\_SB.EC"};
    /* Reproduce a real legacy-to-ACPI transition on disposable Q35. Do not
     * fake original_control: retirement must restore the actual starting mode. */
    if(model.fault==11||model.fault==12) {
        outb(f->smi_cmd,f->acpi_disable);
        if(inw(0x604)&1)return false;
        p->model_mode_entered=model_mode_entered;
    }
    p->events.fixed_enable_mask=1u<<5;p->original_control=inw(0x604);
    p->rsdp=(uintptr_t)rsdp-pmm_hhdm_offset();p->facs=f->firmware_ctrl;
    p->io=(struct ec_io){NULL,status,read_data,command,write_data};
    model.memory[0x60]=3;model.pending=true;
    model.words[0x1a]=0x31;model.words[9]=12000;model.words[10]=(uint16_t)-250;
    model.words[13]=86;model.words[15]=4300;model.words[16]=5000;model.words[24]=6000;
    model.words[8]=2981;model.words[18]=120;model.words[19]=65535;model.words[22]=0xc0;
    if(uacpi_set_table_installation_handler(install))return false;
    if((model.fault>=7&&model.fault<=10)||model.fault>=13) {
        model.foreign_status[0]=model.foreign_enable[0]=0x40;
        model.foreign_status[1]=model.foreign_enable[1]=4;
        p->foreign_events[0]=(struct battery_event_hold){.status=0x627,.enable=0x62f,.mask=0x40,.original=0x40};
        p->foreign_events[1]=(struct battery_event_hold){.status=0x626,.enable=0x62e,.mask=4,.original=4};
        p->foreign_io=(struct battery_event_ops){.read=foreign_read,.write=foreign_write};
    }
    if((model.fault==19||model.fault==27)&&!battery_diagnostic_auto_return(true))return false;
    if(model.fault>=48)p->query_policy=BATTERY_QUERY_AIR72;
    if(model.fault>=62){p->model_smbus_queries=true;p->smbus_capture_profile=true;}
#ifdef SRVROS_BATTERY_AIR61
    p->query_policy=BATTERY_QUERY_AIR61;p->bounded_samples_only=true;
#endif
    if((model.fault>=41 && model.fault<=47) || model.fault==61)p->model_query_checkpoint=model_query_checkpoint;
    if(model.fault==40)p->model_transfer_wait=model_transfer_wait;
    if(model.fault==37||model.fault==91)p->model_owner_irq=model_owner_irq;
    if(model.fault>=33 && model.fault<=40) { p->model_event=model_event;p->model_query_refuse=model_query_refuse; }
    if(model.fault>=30 && model.fault<=40)p->model_irq_fault=model_irq_fault;
    if(model.fault>=13)p->model_sample_begin=model_sample_begin;
    if(model.fault==18||model.fault==19||model.fault==91||model.fault==92)p->model_setup_work=model_setup_work;
    platform=p;console_printf("battery-vm: model fault=%u; no physical EC\n",(uint64_t)model.fault);return true;
}
void *battery_platform_map(uint64_t a,size_t n){
    if(!platform)return NULL;
    if(contains(platform->rsdp,36,a,n)||contains(platform->facs,64,a,n))return pmm_phys_to_virt(a);
    for(uint64_t i=0;i<acpi_table_count();i++){const struct acpi_table_info *t=acpi_table_at(i);if(contains(t->physical_address,t->length,a,n))return pmm_phys_to_virt(a);}
    return NULL;
}
bool battery_platform_memory(void *p,unsigned bits,bool write,uint64_t *v){
    if(!platform||write||bits!=32||(uintptr_t)p-pmm_hhdm_offset()!=platform->facs+16)return false;
    *v=*(volatile uint32_t *)p;return true;
}
bool battery_platform_io_map(uint64_t a,size_t n){(void)a;(void)n;return false;}
bool battery_platform_io(uint64_t a,unsigned b,bool w,uint32_t *v){(void)a;(void)b;(void)w;(void)v;return false;}
bool battery_platform_pci_open(uacpi_pci_address a,void **h){(void)a;(void)h;return false;}
bool battery_platform_pci_read(void *h,size_t o,unsigned b,uint32_t *v){(void)h;(void)o;(void)b;(void)v;return false;}

bool battery_platform_workspace_study_allowed(void) { return true; }
