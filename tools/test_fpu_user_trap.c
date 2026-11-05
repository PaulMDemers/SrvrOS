/* Real unprivileged x86 FXSAVE/FXRSTOR; scheduler/process/CR3 are external models.
 * Critical boundary functions omit sanitizer prologues in the sanitizer profile.
 * Snapshot/consume validation remains instrumented; guard pages cover byte copies.
 * The unsanitized no-SSE run and target disassembly are the save-first evidence. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../kernel/src/arch/x86_64/fpu_trap.h"
#define CRITICAL __attribute__((no_sanitize("address","undefined"),no_instrument_function))
CRITICAL void fpu_common_trap_enter(const struct isr_frame *,uint8_t *,struct fpu_user_trap_proof *);
CRITICAL void fpu_common_trap_exit(const struct isr_frame *,const uint8_t *,struct fpu_user_trap_proof *);
CRITICAL void fpu_trap_enter(uint64_t,uint8_t *);
CRITICAL void fpu_trap_exit(uint64_t,const uint8_t *);
CRITICAL void fpu_save(struct fpu_state *);
CRITICAL void fpu_restore(struct fpu_state *);
CRITICAL void fpu_init_state(struct fpu_state *);
CRITICAL static void copy_fx_area(uint8_t *,const uint8_t *);
static bool canonical_output=true;
static uintptr_t fpu_test_kernel_address(uintptr_t p) {
    return canonical_output ? UINT64_C(0xffff800000000000)|(p&UINT64_C(0x7fffffffffff)) : p;
}
static uint64_t modeled_cr4=(1ull<<9),modeled_efer;
static inline __attribute__((always_inline)) uint64_t fpu_test_read_cr4(void) {return modeled_cr4;}
static inline __attribute__((always_inline)) uint64_t fpu_test_read_efer(void) {return modeled_efer;}
#define SRVROS_FPU_TRAP_HOST_TEST
#include "../kernel/src/arch/x86_64/fpu.c"

static struct {uint8_t head[512];struct fpu_state state;uint8_t tail[512];} ua,ka,ub,kb;
struct common_storage { _Alignas(16) uint8_t bytes[FPU_COMMON_RESERVATION+sizeof(struct isr_frame)+1024]; };
static struct common_storage a,b,nested;
static struct fpu_user_snapshot host_initial,user_image,kernel_image,expected,observed;
static struct scheduler_user_return_observation observation;
static bool entry_ok=true,return_ok=true,resolve_ok=true,prepare_ok=true,query_clobber=true,corrupt_query_owner;
static unsigned entry_calls,return_calls;
static struct isr_frame *frame_of(struct common_storage *s) {return (void *)(s->bytes+FPU_COMMON_RESERVATION);}
static struct fpu_user_trap_proof *proof_of(struct common_storage *s) {return (void *)(s->bytes+FPU_COMMON_PROOF);}
static uint8_t *area_of(struct common_storage *s) {return s->bytes+FPU_COMMON_TRAP_AREA;}
void console_write(const char *s) {(void)s;}
/* Deliberately hostile test callback may use SSE;critical trap helpers
 * remain compiled with the no-SSE policy under test. */
__attribute__((target("sse2"))) bool scheduler_observe_user_entry(struct scheduler_user_return_observation *out) {
    entry_calls++;assert(!user_state_loaded);
    assert(!memcmp(current_user_state->fxsave_area,expected.bytes,512));
    if(query_clobber)__asm__ volatile("pcmpeqd %%xmm15, %%xmm15":::"xmm15");
    if(!entry_ok)return false;
    *out=observation;
    if(corrupt_query_owner)current_user_state=&ub.state;
    return true;
}
bool scheduler_observe_user_return(struct scheduler_user_return_observation *out) {
    return_calls++;if(!return_ok)return false;*out=observation;return true;
}
bool process_resolve_user_binding(void *process,void *thread,uint64_t generation,uint64_t root,struct process_user_binding *out) {
    assert(process==observation.process && thread==observation.user_thread && generation==observation.image_generation && root==observation.address_space);
    if(!resolve_ok)return false;
    *out=(struct process_user_binding){.user_fpu=observation.user_fpu,.fs_base=observation.user_fs_base,
        .kernel_stack_top=thread?0:observation.kernel_stack_top};return true;
}
bool vmm_prepare_user_kernel_mappings(uint64_t root) {assert(root==observation.address_space);return prepare_ok;}

static CRITICAL void save_fx(struct fpu_user_snapshot *out) {
    __asm__ volatile("fxsave64 (%0)"::"r"(out->bytes):"memory");
}
static CRITICAL void load_fx(const struct fpu_user_snapshot *in) {
    __asm__ volatile("fxrstor64 (%0)"::"r"(in->bytes):"memory");
}
static CRITICAL __attribute__((target("sse2"))) void make_image(struct fpu_user_snapshot *out,uint16_t cw,uint32_t mxcsr,const uint64_t *xmm) {
    __asm__ volatile("fninit; fldcw %0; fld1; fldpi; ldmxcsr %1; movdqu %2, %%xmm15"
        ::"m"(cw),"m"(mxcsr),"m"(*(const uint64_t (*)[2])xmm):"memory","xmm15");
    save_fx(out);
}
static CRITICAL void enter_user(struct common_storage *s) {
    user_state_loaded=true;load_fx(&user_image);save_fx(&expected);
    fpu_common_trap_enter(frame_of(s),area_of(s),proof_of(s));
}
static CRITICAL void exit_user(struct common_storage *s) {
    fpu_common_trap_exit(frame_of(s),area_of(s),proof_of(s));save_fx(&observed);
    load_fx(&host_initial);
}
static void select_a(void) {
    observation=(struct scheduler_user_return_observation){.thread_identity=11,.process=(void *)0x1234,
        .address_space=0x2000,.image_generation=9,.user_fpu=&ua.state,.kernel_fpu=&ka.state,
        .user_fs_base=0x700100,.kernel_stack_top=(uintptr_t)frame_of(&a)+sizeof(struct isr_frame)};
}
static void setup(void) {
    memset(&a,0xa5,sizeof(a));memset(&b,0xa5,sizeof(b));memset(&nested,0xa5,sizeof(nested));
    *frame_of(&a)=(struct isr_frame){.cs=0x23,.ss=0x1b,.rip=0x500000,.rsp=0x900000,.rflags=0x202};
    *frame_of(&b)=*frame_of(&a);*frame_of(&nested)=(struct isr_frame){.cs=8};
    memset(&ua,0,sizeof(ua));memset(&ka,0,sizeof(ka));memset(&ub,0,sizeof(ub));memset(&kb,0,sizeof(kb));
    memcpy(ua.state.fxsave_area,kernel_image.bytes,512);memcpy(ka.state.fxsave_area,kernel_image.bytes,512);
    memcpy(ub.state.fxsave_area,kernel_image.bytes,512);memcpy(kb.state.fxsave_area,kernel_image.bytes,512);
    ua.state.initialized=ka.state.initialized=ub.state.initialized=kb.state.initialized=true;
    available=true;sampled_mxcsr_mask=0xffff;sampled_mxcsr_mask_valid=true;current_user_state=&ua.state;current_kernel_state=&ka.state;user_state_loaded=false;
    entry_ok=return_ok=resolve_ok=prepare_ok=canonical_output=query_clobber=true;
    corrupt_query_owner=false;modeled_cr4=(1ull<<9);modeled_efer=0;
    memset(expected.bytes,0,512);memset(observed.bytes,0,512);select_a();
}
static void refused(struct fpu_user_snapshot *out) {
    struct fpu_user_snapshot before;struct fpu_state user_before=ua.state,kernel_before=ka.state;
    struct fpu_user_trap_proof witness=*proof_of(&a);
    memcpy(&before,out,512);
    assert(!fpu_snapshot_user_trap(frame_of(&a),out));
    assert(!memcmp(&before,out,512) && !memcmp(&user_before,&ua.state,sizeof(user_before)) &&
        !memcmp(&kernel_before,&ka.state,sizeof(kernel_before)) && !memcmp(&witness,proof_of(&a),sizeof(witness)));
}
static void restore_refused(const struct fpu_user_snapshot *in) {
    struct fpu_user_snapshot before=*in;
    struct fpu_state user_before=ua.state,kernel_before=ka.state;
    struct fpu_user_trap_proof witness=*proof_of(&a);
    assert(!fpu_restore_saved_user_trap(frame_of(&a),in));
    assert(!memcmp(&before,in,512) && !memcmp(&user_before,&ua.state,sizeof(user_before)) &&
        !memcmp(&kernel_before,&ka.state,sizeof(kernel_before)) && !memcmp(&witness,proof_of(&a),sizeof(witness)));
}
static CRITICAL void nested_roundtrip(bool direct,struct fpu_user_snapshot *before,struct fpu_user_snapshot *after) {
    save_fx(before);
    if(direct)fpu_trap_enter(8,area_of(&nested));
    else fpu_common_trap_enter(frame_of(&nested),area_of(&nested),proof_of(&nested));
    load_fx(&user_image);
    if(direct)fpu_trap_exit(8,area_of(&nested));
    else fpu_common_trap_exit(frame_of(&nested),area_of(&nested),proof_of(&nested));
    save_fx(after);
}
static CRITICAL void short_roundtrip(struct isr_frame *f,uint8_t *area,struct fpu_user_trap_proof *p,
    struct fpu_user_snapshot *before,struct fpu_user_snapshot *after) {
    save_fx(before);fpu_common_trap_enter(f,area,p);load_fx(&user_image);
    fpu_common_trap_exit(f,area,p);save_fx(after);
}

int main(void) {
    save_fx(&host_initial);
    memcpy(initial_fxsave_area,host_initial.bytes,512);
    const uint64_t up[2]={UINT64_C(0x1122334455667788),UINT64_C(0x8877665544332211)};
    const uint64_t kp[2]={UINT64_C(0xaabbccddeeff0011),UINT64_C(0x1100ffeeddccbbaa)};
    make_image(&user_image,0x0b7f,0x3f80,up);make_image(&kernel_image,0x037f,0x1f80,kp);load_fx(&host_initial);
    long page=sysconf(_SC_PAGESIZE);assert(page>=4096);
    uint8_t *guard=mmap(NULL,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);assert(guard!=MAP_FAILED);
    assert(!mprotect(guard+page,(size_t)page,PROT_NONE));
    struct fpu_user_snapshot *out=(void *)(guard+page-512);
    setup();enter_user(&a);
    assert(proof_of(&a)->state==FPU_PROOF_VALID && ka.state.active_user_trap==proof_of(&a));
    struct fpu_state before_validate_user=ua.state,before_validate_kernel=ka.state;
    struct fpu_user_trap_proof before_validate_proof=*proof_of(&a);
    assert(fpu_validate_user_trap(frame_of(&a)) && fpu_validate_user_trap(frame_of(&a)));
    assert(!memcmp(&before_validate_user,&ua.state,sizeof(ua.state)) &&
        !memcmp(&before_validate_kernel,&ka.state,sizeof(ka.state)) &&
        !memcmp(&before_validate_proof,proof_of(&a),sizeof(before_validate_proof)));
    assert(!memcmp(ua.state.fxsave_area,expected.bytes,512));
    for(unsigned i=0;i<512;i++)assert(area_of(&a)[i]==0xa5);
    assert(fpu_snapshot_user_trap(frame_of(&a),out) && !memcmp(out->bytes,expected.bytes,512));
    struct fpu_user_snapshot before={0},after={0};unsigned calls=entry_calls;
    nested_roundtrip(false,&before,&after);assert(!memcmp(&before,&after,512) && ka.state.active_user_trap==proof_of(&a) && entry_calls==calls);
    nested_roundtrip(true,&before,&after);assert(!memcmp(&before,&after,512) && ka.state.active_user_trap==proof_of(&a) && entry_calls==calls);
    uint8_t *short_map=mmap(NULL,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);assert(short_map!=MAP_FAILED);
    assert(!mprotect(short_map+page,(size_t)page,PROT_NONE));
    struct isr_frame *short_frame=(void *)(short_map+page - offsetof(struct isr_frame,rsp));
    memset(short_frame,0,offsetof(struct isr_frame,rsp));short_frame->cs=8;
    uintptr_t floor=(uintptr_t)short_frame&~(uintptr_t)15;
    short_roundtrip(short_frame,(void *)(floor-FPU_COMMON_RESERVATION+FPU_COMMON_TRAP_AREA),
        (void *)(floor-64),&before,&after);
    assert(!memcmp(&before,&after,512) && ka.state.active_user_trap==proof_of(&a) && entry_calls==calls);
    assert(!fpu_snapshot_user_trap(short_frame,out) && !fpu_validate_user_trap(short_frame));assert(!munmap(short_map,(size_t)page*2));
    refused((void *)&ua.state);refused((void *)((uint8_t *)&ua.state+16));
    refused((void *)((uint8_t *)&ua.state+512));refused((void *)&ka.state);
    refused((void *)proof_of(&a));refused((void *)frame_of(&a));
    return_ok=false;refused(out);return_ok=true;
    resolve_ok=false;refused(out);resolve_ok=true;prepare_ok=false;refused(out);prepare_ok=true;
    canonical_output=false;refused(out);canonical_output=true;
    uint64_t id=observation.thread_identity;observation.thread_identity++;refused(out);observation.thread_identity=id;
    observation.image_generation++;refused(out);observation.image_generation--;
    observation.address_space+=4096;refused(out);observation.address_space-=4096;
    ka.state.active_user_trap=(void *)1;refused(out);ka.state.active_user_trap=proof_of(&a);
    current_user_state=(void *)1;refused(out);current_user_state=&ua.state;
    user_state_loaded=true;calls=return_calls;refused(out);assert(return_calls==calls);user_state_loaded=false;
    struct fpu_user_snapshot unchanged=*out;
    assert(!fpu_snapshot_user_trap(NULL,out) && !fpu_snapshot_user_trap(frame_of(&a),NULL));
    assert(!fpu_snapshot_user_trap(frame_of(&a),(void *)((uintptr_t)out+1)));
    assert(!fpu_snapshot_user_trap(frame_of(&a),(void *)(UINTPTR_MAX&~(uintptr_t)15)) && !memcmp(out,&unchanged,512));
    assert(fpu_consume_user_trap(frame_of(&a)) && proof_of(&a)->state==FPU_PROOF_CONSUMED);
    refused(out);assert(!fpu_consume_user_trap(frame_of(&a)) && !fpu_validate_user_trap(frame_of(&a)));
    exit_user(&a);assert(!ka.state.active_user_trap && !memcmp(observed.bytes,expected.bytes,512));

    const uint64_t denied_cr4[]={0,(1ull<<9)|(1ull<<18),(1ull<<9)};
    const uint64_t denied_efer[]={0,0,(1ull<<14)};
    for(unsigned i=0;i<3;i++) {
        setup();modeled_cr4=denied_cr4[i];modeled_efer=denied_efer[i];calls=entry_calls;
        enter_user(&a);
        assert(!ka.state.active_user_trap && proof_of(&a)->state==FPU_PROOF_INVALID && entry_calls==calls);
        refused(out);exit_user(&a);assert(!ka.state.active_user_trap);
        setup();enter_user(&a);
        modeled_cr4=denied_cr4[i];modeled_efer=denied_efer[i];calls=return_calls;
        refused(out);struct fpu_user_trap_proof unchanged_proof=*proof_of(&a);
        assert(!fpu_consume_user_trap(frame_of(&a)) && return_calls==calls &&
            !memcmp(&unchanged_proof,proof_of(&a),sizeof(unchanged_proof)) && ka.state.active_user_trap==proof_of(&a));
        exit_user(&a);assert(!ka.state.active_user_trap);
    }

    setup();entry_ok=false;enter_user(&a);assert(!ka.state.active_user_trap && proof_of(&a)->state==FPU_PROOF_INVALID);refused(out);exit_user(&a);
    setup();corrupt_query_owner=true;enter_user(&a);
    assert(!ka.state.active_user_trap && proof_of(&a)->state==FPU_PROOF_INVALID);
    current_user_state=&ua.state;exit_user(&a);
    setup();observation.user_fpu=&ub.state;enter_user(&a);
    assert(!ka.state.active_user_trap && proof_of(&a)->state==FPU_PROOF_INVALID);select_a();exit_user(&a);
    setup();observation.kernel_fpu=&kb.state;enter_user(&a);
    assert(!ka.state.active_user_trap && proof_of(&a)->state==FPU_PROOF_INVALID);select_a();exit_user(&a);
    setup();ua.state.initialized=false;enter_user(&a);
    assert(!ka.state.active_user_trap && proof_of(&a)->state==FPU_PROOF_INVALID);refused(out);exit_user(&a);
    setup();ka.state.initialized=false;enter_user(&a);
    assert(!ka.state.active_user_trap && proof_of(&a)->state==FPU_PROOF_INVALID);refused(out);exit_user(&a);
    setup();available=false;enter_user(&a);
    assert(!ka.state.active_user_trap && proof_of(&a)->state==FPU_PROOF_INVALID);refused(out);exit_user(&a);
    setup();user_state_loaded=false;fpu_common_trap_enter(frame_of(&a),area_of(&a),proof_of(&a));
    assert(!ka.state.active_user_trap && proof_of(&a)->state==FPU_PROOF_INVALID);refused(out);exit_user(&a);
    setup();ka.state.active_user_trap=(void *)1;enter_user(&a);assert(ka.state.active_user_trap==(void *)1 && proof_of(&a)->state==FPU_PROOF_INVALID);refused(out);exit_user(&a);fpu_invalidate_user_trap(&ka.state);
    setup();enter_user(&a);
    fpu_switch_kernel_state(&ka.state,&kb.state,&ub.state);
    observation.user_fpu=&ub.state;observation.kernel_fpu=&kb.state;observation.thread_identity=12;observation.image_generation=10;observation.address_space=0x3000;
    enter_user(&b);assert(kb.state.active_user_trap==proof_of(&b));
    fpu_switch_kernel_state(&kb.state,&ka.state,&ua.state);select_a();
    assert(ka.state.active_user_trap==proof_of(&a) && fpu_snapshot_user_trap(frame_of(&a),out));
    exit_user(&a);assert(kb.state.active_user_trap==proof_of(&b));fpu_invalidate_user_trap(&kb.state);
    /* The input is the immutable snapshot captured by the real entry API,
     * retained privately while a later handler entry saves different state. */
    struct fpu_user_snapshot original_pattern=user_image,private_saved,invalid;
    setup();enter_user(&a);assert(fpu_snapshot_user_trap(frame_of(&a),&private_saved));exit_user(&a);
    user_image=kernel_image;setup();enter_user(&a);
    sampled_mxcsr_mask_valid=false;restore_refused(&private_saved);sampled_mxcsr_mask_valid=true;
    for(unsigned bit=0;bit<32;bit++) {
        /* Each bit is unsupported in the sampled mask or architecturally high. */
        sampled_mxcsr_mask=bit<16 ? (0xffffu & ~(1u<<bit)) : UINT32_MAX;
        invalid=private_saved;
        for(unsigned byte=0;byte<4;byte++) invalid.bytes[24+byte]=0;
        invalid.bytes[24+bit/8]=(uint8_t)(1u<<(bit%8));
        memset(invalid.bytes+28,0xff,4); /* Source mask is never authority. */
        restore_refused(&invalid);
    }
    sampled_mxcsr_mask=0xffbf;invalid=private_saved;invalid.bytes[24]|=0x40;
    restore_refused(&invalid);sampled_mxcsr_mask=0xffff;
    restore_refused((void *)((uint8_t *)&ua.state-496));
    restore_refused((void *)((uint8_t *)&ka.state-496));
    restore_refused((void *)&ua.state);restore_refused((void *)((uint8_t *)&ua.state+16));
    restore_refused((void *)((uint8_t *)&ua.state+512));restore_refused((void *)&ka.state);
    restore_refused((void *)((uint8_t *)&ka.state+512));
    restore_refused((void *)proof_of(&a));restore_refused((void *)frame_of(&a));
    assert(!fpu_restore_saved_user_trap(NULL,&private_saved) && !fpu_restore_saved_user_trap(frame_of(&a),NULL));
    assert(!fpu_restore_saved_user_trap(frame_of(&a),(void *)((uintptr_t)&private_saved+1)));
    assert(!fpu_restore_saved_user_trap(frame_of(&a),(void *)(UINTPTR_MAX&~(uintptr_t)15)));
    canonical_output=false;restore_refused(&private_saved);canonical_output=true;
    return_ok=false;restore_refused(&private_saved);return_ok=true;
    resolve_ok=false;restore_refused(&private_saved);resolve_ok=true;
    prepare_ok=false;restore_refused(&private_saved);prepare_ok=true;
    observation.image_generation++;restore_refused(&private_saved);observation.image_generation--;
    observation.thread_identity++;restore_refused(&private_saved);observation.thread_identity--;
    observation.address_space+=4096;restore_refused(&private_saved);observation.address_space-=4096;
    ka.state.active_user_trap=(void *)1;restore_refused(&private_saved);ka.state.active_user_trap=proof_of(&a);
    current_user_state=(void *)1;restore_refused(&private_saved);current_user_state=&ua.state;
    for(unsigned i=0;i<3;i++) {
        modeled_cr4=denied_cr4[i];modeled_efer=denied_efer[i];restore_refused(&private_saved);
        modeled_cr4=(1ull<<9);modeled_efer=0;
    }
    ua.state.initialized=false;restore_refused(&private_saved);ua.state.initialized=true;
    ka.state.initialized=false;restore_refused(&private_saved);ka.state.initialized=true;
    user_state_loaded=true;restore_refused(&private_saved);user_state_loaded=false;
    available=false;restore_refused(&private_saved);available=true;
    memcpy(out,&private_saved,512); /* Source ends exactly at the protected page. */
    struct fpu_state saved_kernel=ka.state;bool initialized_user=ua.state.initialized;
    assert(fpu_restore_saved_user_trap(frame_of(&a),out));
    assert(!memcmp(ua.state.fxsave_area,private_saved.bytes,512) && ua.state.initialized==initialized_user &&
        !ua.state.active_user_trap && !memcmp(&saved_kernel,&ka.state,sizeof(saved_kernel)) &&
        proof_of(&a)->state==FPU_PROOF_CONSUMED);
    restore_refused(&private_saved);refused(out);assert(!fpu_consume_user_trap(frame_of(&a)));
    exit_user(&a);assert(!memcmp(observed.bytes,private_saved.bytes,512) && !ka.state.active_user_trap);
    /* Optional DAZ is accepted only as an explicit modeled capability; perform
     * a final hardware restore only when this host actually advertises it. */
    uint32_t host_mask=0;memcpy(&host_mask,host_initial.bytes+28,4);
    if(host_mask&(1u<<6)) {
        setup();enter_user(&a);sampled_mxcsr_mask=host_mask;
        invalid=private_saved;invalid.bytes[24]|=0x40;
        assert(fpu_restore_saved_user_trap(frame_of(&a),&invalid));exit_user(&a);
        assert(!memcmp(observed.bytes,invalid.bytes,512));
    }
    /* Preparation is read-only until one explicit, same-continuation commit. */
    setup();enter_user(&a);
    struct fpu_user_trap_commit prepared_commit, untouched;
    memset(&prepared_commit,0xa5,sizeof(prepared_commit));untouched=prepared_commit;
    struct fpu_state before_user=ua.state,before_kernel=ka.state;
    struct fpu_user_trap_proof before_proof=*proof_of(&a);
    invalid=private_saved;invalid.bytes[27]=1;
    assert(!fpu_prepare_user_trap_commit(frame_of(&a),&invalid,&prepared_commit));
    assert(!memcmp(&prepared_commit,&untouched,sizeof(untouched)) &&
        !memcmp(&before_user,&ua.state,sizeof(before_user)) && !memcmp(&before_kernel,&ka.state,sizeof(before_kernel)) &&
        !memcmp(&before_proof,proof_of(&a),sizeof(before_proof)));
    assert(!fpu_prepare_user_trap_commit(frame_of(&a),&private_saved,(void *)((uintptr_t)&prepared_commit+1)));
    assert(!fpu_prepare_user_trap_commit(frame_of(&a),&private_saved,(void *)(UINTPTR_MAX&~(uintptr_t)15)));
    assert(!fpu_prepare_user_trap_commit(frame_of(&a),&private_saved,(void *)&ua.state));
    assert(!fpu_prepare_user_trap_commit(frame_of(&a),&private_saved,(void *)proof_of(&a)));
    assert(!fpu_prepare_user_trap_commit(frame_of(&a),&private_saved,(void *)&private_saved));
    assert(!fpu_prepare_user_trap_commit(frame_of(&a),(void *)&ka.state,&prepared_commit));
    observation.image_generation++;assert(!fpu_prepare_user_trap_commit(frame_of(&a),NULL,&prepared_commit));observation.image_generation--;
    ka.state.active_user_trap=(void *)1;assert(!fpu_prepare_user_trap_commit(frame_of(&a),NULL,&prepared_commit));ka.state.active_user_trap=proof_of(&a);
    assert(fpu_prepare_user_trap_commit(frame_of(&a),NULL,&prepared_commit));
    assert(prepared_commit.proof==proof_of(&a) && prepared_commit.user_fpu==&ua.state && !prepared_commit.restore);
    assert(!memcmp(&before_user,&ua.state,sizeof(before_user)) && !memcmp(&before_kernel,&ka.state,sizeof(before_kernel)) &&
        !memcmp(&before_proof,proof_of(&a),sizeof(before_proof)));
    /* Abandon this preparation: it must leave the proof usable. */
    assert(fpu_prepare_user_trap_commit(frame_of(&a),&private_saved,&prepared_commit));
    assert(proof_of(&a)->state==FPU_PROOF_VALID);
    fpu_commit_user_trap_prepared(&prepared_commit);
    assert(proof_of(&a)->state==FPU_PROOF_CONSUMED && !memcmp(ua.state.fxsave_area,private_saved.bytes,512));
    assert(ua.state.initialized==before_user.initialized && ua.state.active_user_trap==before_user.active_user_trap &&
        !memcmp(&before_kernel,&ka.state,sizeof(before_kernel)));
    assert(!fpu_prepare_user_trap_commit(frame_of(&a),NULL,&prepared_commit));
    exit_user(&a);assert(!memcmp(observed.bytes,private_saved.bytes,512));
    setup();enter_user(&a);
    assert(fpu_prepare_user_trap_commit(frame_of(&a),NULL,&prepared_commit));
    before_user=ua.state;fpu_commit_user_trap_prepared(&prepared_commit);
    assert(proof_of(&a)->state==FPU_PROOF_CONSUMED && !memcmp(&before_user,&ua.state,sizeof(before_user)));
    exit_user(&a);
    user_image=original_pattern;
    user_state_loaded=false;current_kernel_state=current_user_state=NULL;load_fx(&host_initial);
    assert(!munmap(guard,(size_t)page*2));
    puts("fpu-user-trap: PASS real FXSAVE/FXRSTOR x87/MXCSR/XMM, save-before-query, nesting, aliases, lifetime, consume and private restore");
    return 0;
}
