#include <srvros/ap_probe.h>
#include <srvros/console.h>
#include <srvros/idle_audit.h>
#include <srvros/timer.h>
#include <limine.h>
#include <stddef.h>

#define AP_MAX 4
_Static_assert(AP_PROBE_ROUNDS==SCHEDULER_MWAIT_ATTEMPTS && AP_PROBE_LIMIT_US==SCHEDULER_MWAIT_LIMIT_US,"existing limits");
struct __attribute__((packed)) desc { uint16_t limit; uint64_t base; };
struct __attribute__((packed)) gate { uint16_t low, cs; uint8_t ist, attr; uint16_t mid; uint32_t high, zero; };
struct __attribute__((packed)) tss {
    uint32_t zero; uint64_t rsp[3], reserved, ist[7], reserved2;
    uint16_t reserved3, iomap;
};
struct slot {
    _Alignas(64) uint64_t command;
    unsigned char pad[56];
    _Alignas(64) uint64_t ack;
    uint64_t state, fault, apic, signature, monitor, waits, inject;
    struct desc gdtr; unsigned char pad2[6];
    struct desc idtr; unsigned char pad3[6];
    uint64_t stack_top;
    uint64_t c3_mode, armed, done, deep_count, early, tsc_delta, c3_delta;
    uint64_t config_before, config_after, c3_cap;
    _Alignas(16) uint64_t gdt[5];
    struct tss tss;
    _Alignas(16) struct gate idt[256];
    _Alignas(16) unsigned char stack[16384], emergency[16384];
};
_Static_assert(offsetof(struct slot,ack)==64 && offsetof(struct slot,gdtr)==128 &&
               offsetof(struct slot,idtr)==144 && offsetof(struct slot,stack_top)==160,
               "AP assembly layout");
_Static_assert(offsetof(struct slot,c3_mode)==168 && offsetof(struct slot,armed)==176 &&
               offsetof(struct slot,config_after)==232 && offsetof(struct slot,c3_cap)==240,"C3 assembly layout");
_Static_assert(sizeof(struct tss)==104,"x86 TSS layout");
static struct slot slots[AP_MAX];
static bool used, owner_pending, final_pass;
static unsigned count, coord_mode, coord_rounds, coord_fault;
static struct limine_mp_info *info[AP_MAX];
static uint64_t startup[AP_MAX],max[AP_MAX],retire[AP_MAX];
static unsigned rounds[AP_MAX];
static uint64_t coord_epoch,coord_config;
struct coord_row { uint64_t us,tsc,core,package,peer_tsc[3],peer_core[3]; };
static struct coord_row coord_rows[AP_PROBE_ROUNDS];
static uint64_t read64(unsigned reg) {
    unsigned a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(reg));return a|((uint64_t)d<<32);
}
extern volatile struct limine_mp_request ap_probe_request;
extern void ap_probe_entry(struct limine_mp_info *);
extern void ap_probe_fault(void);
static uint64_t get(uint64_t *p) { return __atomic_load_n(p,__ATOMIC_ACQUIRE); }
static void put(uint64_t *p,uint64_t v) { __atomic_store_n(p,v,__ATOMIC_RELEASE); }
static void cpuid(unsigned leaf,unsigned *a,unsigned *b,unsigned *c,unsigned *d) {
    __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));
}
static bool token(const char *s,const char *want) {
    for(;*s;s++) { if(s[0]==' ')continue;const char *a=s,*b=want;
        while(*b && *a==*b){a++;b++;}if(!*b && (!*a || *a==' '))return true;
        while(*s && *s!=' ')s++;if(!*s)break;
    }return false;
}
static void prepare(struct slot *s,bool monitor,unsigned inject) {
    s->stack_top=(uint64_t)(s->stack+sizeof(s->stack));
    s->monitor=monitor;s->inject=inject;
    s->gdt[1]=0x00af9a000000ffffull;s->gdt[2]=0x00cf92000000ffffull;
    uint64_t t=(uint64_t)&s->tss,limit=sizeof(s->tss)-1;
    s->gdt[3]=limit|((t&0xffffff)<<16)|(0x89ull<<40)|((t&0xff000000)<<32);
    s->gdt[4]=t>>32;
    s->tss.rsp[0]=s->stack_top;s->tss.ist[0]=(uint64_t)(s->emergency+sizeof(s->emergency));
    s->tss.iomap=sizeof(s->tss);
    s->gdtr=(struct desc){sizeof(s->gdt)-1,(uint64_t)s->gdt};
    for(unsigned i=0;i<256;i++) {
        uint64_t h=(uint64_t)ap_probe_fault;
        s->idt[i]=(struct gate){h,8,1,0x8e,h>>16,h>>32,0};
    }
    s->idtr=(struct desc){sizeof(s->idt)-1,(uint64_t)s->idt};
}
/* BSP only. IRQ masking bounds measurement without allowing scheduler entry on
 * APs. A second spin bound handles a stopped reference. No console or allocation
 * while masked. Timeout never frees/reuses slots or Limine's retained storage. */
static bool wait_value(struct slot *s,bool state,uint64_t value,uint64_t *maximum,struct limine_mp_info *launch,bool send) {
    uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
    uint64_t start=timer_monotonic_us();bool ok=false;
    if(launch)__atomic_store_n(&launch->goto_address,ap_probe_entry,__ATOMIC_RELEASE);
    if(send)put(&s->command,state?AP_PROBE_STOP:value);
    for(unsigned spin=0;spin<1000000;spin++) {
        uint64_t now=timer_monotonic_us(),ack=get(state?&s->state:&s->ack);
        int result=ap_probe_observe(value,ack,get(&s->fault),now-start,now>=start);
        if(now>=start && now-start>*maximum)*maximum=now-start;
        if(result){ok=result>0;break;}
        __asm__ volatile("pause");
    }
    if(flags&(1ull<<9))__asm__ volatile("sti":::"memory");
    return ok;
}
bool ap_probe_run(const char *cmdline) {
    if(used || !timer_reference_ready()) { console_write("ap-owner: v=1 admitted=0 reason=lifetime-clock\n");return false; }
    used=true;
    bool requested=token(cmdline,"srvros.ap=c3-test");
    unsigned a,b,c,d;cpuid(0,&a,&b,&c,&d);
    bool intel=b==0x756e6547 && d==0x49656e69 && c==0x6c65746e;
    unsigned maxleaf=a;cpuid(1,&a,&b,&c,&d);
    uint32_t signature=a;bool vm=c&(1u<<31),monitor=c&(1u<<3);
    unsigned bsp=b>>24;
    if(monitor && maxleaf>=5) { cpuid(5,&a,&b,&c,&d);monitor=a==64 && b==64 && (c&1) && (d&0xf0); }
    else monitor=false;
    struct limine_mp_response *r=ap_probe_request.response;
    bool valid=r && r->cpus && bsp<AP_MAX && r->revision==0 && r->flags==0 && r->cpu_count>=2 && r->cpu_count<=AP_MAX && r->bsp_lapic_id==bsp;
    uint64_t mask=0;
    if(valid) for(uint64_t i=0;i<r->cpu_count;i++) {
        struct limine_mp_info *p=r->cpus[i];
        if(!p || p->lapic_id>=AP_MAX || (mask&(1ull<<p->lapic_id)) || p->goto_address) {valid=false;break;}
        mask|=1ull<<p->lapic_id;
    }
    valid=valid && (mask&(1ull<<bsp));
    if(!vm)valid=valid && intel && signature==0x40651 && monitor && bsp==0 && r->cpu_count==4 && mask==15;
    if(!valid) {console_write("ap-owner: v=1 admitted=0 reason=identity-topology\n");return false;}
    bool simulation=vm && requested && token(cmdline,"srvros.ap-protocol=simulate");
    if(requested) {
        bool gated=simulation;
        if(!vm) {
            cpuid(5,&a,&b,&c,&d);gated=(c&3)==3 && ((d>>8)&15);
            cpuid(6,&a,&b,&c,&d);gated=gated && (a&(1u<<2));
            cpuid(0x80000000,&a,&b,&c,&d);gated=gated && a>=0x80000007;
            cpuid(0x80000007,&a,&b,&c,&d);gated=gated && (d&(1u<<8));
            gated=gated && timer_idle_wakeup_ready();
            if(gated) {coord_config=read64(0xe2);gated=(coord_config&15)>=2 && (coord_config&15)<=8;}
        }
        if(!gated || count || r->cpu_count!=4 || mask!=15) {
            console_write("ap-c3: v=1 admitted=0 reason=native-gates\n");return false;
        }
        coord_mode=simulation?2:1;
    }
    unsigned inject=vm && token(cmdline,"srvros.ap-inject=missing")?1:vm && token(cmdline,"srvros.ap-inject=fault")?2:0;
    console_printf("ap-owner: v=1 admitted=1 mode=%s cpus=%u bsp=%u mask=%u method=%s rounds=32 limit_us=20000 retained=yes\n",
        vm?"vm":"native",r->cpu_count,(uint64_t)bsp,mask,monitor?"monitor-c1":"pause");
    if(simulation) {
        if(token(cmdline,"srvros.ap-inject=c3-missing"))inject=3;
        if(token(cmdline,"srvros.ap-inject=c3-early"))inject=4;
        if(token(cmdline,"srvros.ap-inject=c3-fault"))inject=5;
    }
    for(uint64_t i=0;i<r->cpu_count;i++) if(r->cpus[i]->lapic_id!=bsp) {
        info[count]=r->cpus[i];prepare(&slots[count],monitor,count==0?inject:0);slots[count].c3_mode=coord_mode;count++;
    }
    bool ok=true;owner_pending=true;
    for(unsigned i=0;i<count;i++) {
        info[i]->extra_argument=(uint64_t)&slots[i];
    }
    for(unsigned i=0;i<count;i++) {
        bool ready=wait_value(&slots[i],true,1,&startup[i],info[i],false);
        ok=ok && ready && get(&slots[i].apic)==info[i]->lapic_id && get(&slots[i].signature)==signature;
    }
    for(uint64_t seq=1;seq<=AP_PROBE_ROUNDS && ok;seq++) {
        /* Give APs time to arm MONITOR. This is not counted as wake latency. */
        if(!timer_delay_us(1000)){ok=false;break;}
        for(unsigned i=0;i<count;i++) {
            if(!wait_value(&slots[i],false,seq,&max[i],NULL,true)) {ok=false;break;}
            rounds[i]++;
        }
    }
    if(coord_mode && ok) {
        for(unsigned i=0;i<count;i++) if(coord_mode==1 && !get(&slots[i].c3_cap))ok=false;
        if(ok && simulation) {
            for(uint64_t seq=1;seq<=AP_PROBE_ROUNDS && !coord_fault;seq++) {
                uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
                uint64_t start=timer_monotonic_us();
                bool armed=ap_c3_prepare(1,seq,start);
                if(armed) { (void)timer_delay_us(1000); (void)ap_c3_finish(seq,start,0,0,0,true); }
                if(flags&(1ull<<9))__asm__ volatile("sti":::"memory");
            }
        }
        if(ok)return true;
    }
    if(coord_mode)ap_c3_cancel();
    bool retired=ap_c3_stop();return ok && retired;
}

static bool coord_fail(unsigned code) {
    if(!coord_fault)coord_fault=code;
    /* Unconditional store breaks any AP monitored wait, even after a deadline.
     * No subsequent wait/free in this IRQ-masked failure cleanup. */
    for(unsigned i=0;i<count;i++)put(&slots[i].command,AP_PROBE_STOP);
    return false;
}
void ap_c3_cancel(void) { (void)coord_fail(1); }
bool ap_c3_active(void) { return coord_mode && owner_pending && !coord_fault && coord_rounds<AP_PROBE_ROUNDS; }
static bool within(uint64_t start) {
    uint64_t now=timer_monotonic_us();return now>=start && now-start<=AP_PROBE_LIMIT_US;
}
bool ap_c3_prepare(uint64_t epoch,uint64_t seq,uint64_t start) {
    if(!ap_c3_active() || !epoch || seq!=coord_rounds+1 || (coord_epoch && coord_epoch!=epoch))return coord_fail(2);
    coord_epoch=epoch;
    for(unsigned i=0;i<count;i++)put(&slots[i].command,256+seq);
    for(unsigned spin=0;spin<1000000;spin++) {
        if(!within(start))return coord_fail(3);
        bool all=true;
        for(unsigned i=0;i<count;i++) {
            struct slot *s=&slots[i];uint64_t armed=get(&s->armed);
            if(get(&s->fault) || get(&s->state)!=1 || armed>seq || get(&s->done)>=seq)return coord_fail(4);
            if(armed!=seq)all=false;
            else if(coord_mode==1 && get(&s->config_before)!=coord_config)return coord_fail(5);
        }
        if(all)return true;
        __asm__ volatile("pause");
    }
    return coord_fail(3);
}
bool ap_c3_finish(uint64_t seq,uint64_t start,uint64_t tsc,uint64_t core,uint64_t package,bool waited) {
    /* Always release peers before checking BSP data/deadline. */
    for(unsigned i=0;i<count;i++)put(&slots[i].command,512+seq);
    if(!ap_c3_active() || seq!=coord_rounds+1 || !waited ||
       (coord_mode==1 && (!tsc || core>tsc || package>tsc)))return coord_fail(6);
    for(unsigned spin=0;spin<1000000;spin++) {
        if(!within(start))return coord_fail(3);
        bool all=true;
        for(unsigned i=0;i<count;i++) {
            struct slot *s=&slots[i];uint64_t done=get(&s->done),ack=get(&s->ack);
            if(get(&s->fault) || get(&s->state)!=1 || get(&s->early) || done>seq || ack>512+seq)return coord_fail(7);
            if(done!=seq || ack!=512+seq)all=false;
        }
        if(all) {
            struct coord_row *row=&coord_rows[coord_rounds];
            row->tsc=tsc;row->core=core;row->package=package;
            for(unsigned i=0;i<count;i++) {
                struct slot *s=&slots[i];row->peer_tsc[i]=get(&s->tsc_delta);row->peer_core[i]=get(&s->c3_delta);
                if(get(&s->deep_count)!=seq || (coord_mode==1 &&
                   (!row->peer_tsc[i] || row->peer_core[i]>row->peer_tsc[i] ||
                    get(&s->config_before)!=coord_config || get(&s->config_after)!=coord_config)))return coord_fail(8);
            }
            uint64_t end=timer_monotonic_us();
            if(end<start || end-start>AP_PROBE_LIMIT_US)return coord_fail(3);
            row->us=end-start;coord_rounds++;return true;
        }
        __asm__ volatile("pause");
    }
    return coord_fail(3);
}
bool ap_c3_stop(void) {
    if(!owner_pending)return final_pass;
    bool ok=true;
    for(unsigned i=0;i<count;i++) {
        bool stopped=wait_value(&slots[i],true,2,&retire[i],NULL,true);
        ok=ok && stopped && get(&slots[i].ack)==AP_PROBE_STOP && rounds[i]==AP_PROBE_ROUNDS && (!slots[i].monitor || get(&slots[i].waits)>0);
        console_printf("ap-owner-cpu: id=%u seen=%u signature=%u ready_us=%u rounds=%u max_us=%u retire_us=%u state=%u fault=%u waits=%u\n",
            (uint64_t)info[i]->lapic_id,get(&slots[i].apic),get(&slots[i].signature),startup[i],(uint64_t)rounds[i],max[i],retire[i],get(&slots[i].state),get(&slots[i].fault),get(&slots[i].waits));
    }
    console_printf("ap-owner: completed=%s retired=%s memory=retained policy_writes=0\n",ok?"yes":"NO",ok?"yes":"see-cpu-receipts");
    if(coord_mode) {
        ok=ok && !coord_fault && coord_rounds==AP_PROBE_ROUNDS;
        console_printf("ap-c3: v=1 mode=%s epoch=%u rounds=%u fault=%u config=%u limit_us=20000 completed=%s retained=yes\n",
            coord_mode==1?"native":"vm-simulation",coord_epoch,(uint64_t)coord_rounds,(uint64_t)coord_fault,coord_config,ok?"yes":"NO");
        for(unsigned i=0;i<count;i++) console_printf("ap-c3-peer: id=%u mode=%u armed=%u done=%u attempts=%u early=%u config=%u,%u\n",
            (uint64_t)info[i]->lapic_id,slots[i].c3_mode,get(&slots[i].armed),get(&slots[i].done),get(&slots[i].deep_count),get(&slots[i].early),get(&slots[i].config_before),get(&slots[i].config_after));
        for(unsigned n=0;n<coord_rounds;n++) {
            const struct coord_row *v=&coord_rows[n];
            console_printf("ap-c3-round: seq=%u us=%u tsc=%u core=%u package=%u p0=%u,%u,%u p1=%u,%u,%u p2=%u,%u,%u\n",
                (uint64_t)n+1,v->us,v->tsc,v->core,v->package,
                (uint64_t)info[0]->lapic_id,v->peer_tsc[0],v->peer_core[0],
                (uint64_t)info[1]->lapic_id,v->peer_tsc[1],v->peer_core[1],
                (uint64_t)info[2]->lapic_id,v->peer_tsc[2],v->peer_core[2]);
        }
    }
    owner_pending=false;final_pass=ok;return ok;
}
