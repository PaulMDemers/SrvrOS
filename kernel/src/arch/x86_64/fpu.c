#include <srvros/console.h>
#include <srvros/fpu.h>

#include <stddef.h>
#include <stdint.h>

#define CPUID_FEATURE_FXSR (1u << 24)
#define CPUID_FEATURE_SSE (1u << 25)
#define CPUID_FEATURE_SSE2 (1u << 26)

#define CR0_MP (1ull << 1)
#define CR0_EM (1ull << 2)
#define CR0_TS (1ull << 3)
#define CR0_NE (1ull << 5)

#define CR4_OSFXSR (1ull << 9)
#define CR4_OSXMMEXCPT (1ull << 10)

static bool available;
static uint32_t sampled_mxcsr_mask;
static bool sampled_mxcsr_mask_valid;
static uint8_t initial_fxsave_area[FPU_FXSAVE_AREA_SIZE] __attribute__((aligned(16)));
static struct fpu_state *current_kernel_state;
static struct fpu_state *current_user_state;
static bool user_state_loaded;

static void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0));
}

static uint64_t read_cr0(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(value));
    return value;
}

static void write_cr0(uint64_t value) {
    __asm__ volatile ("mov %0, %%cr0" : : "r"(value) : "memory");
}

static uint64_t read_cr4(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(value));
    return value;
}

static void write_cr4(uint64_t value) {
    __asm__ volatile ("mov %0, %%cr4" : : "r"(value) : "memory");
}

static void fxsave(uint8_t *area) {
    __asm__ volatile ("fxsave64 (%0)" : : "r"(area) : "memory");
}

static void fxrstor(const uint8_t *area) {
    __asm__ volatile ("fxrstor64 (%0)" : : "r"(area) : "memory");
}

static void copy_fx_area(uint8_t *destination, const uint8_t *source) {
    for (uint64_t i = 0; i < FPU_FXSAVE_AREA_SIZE; i++) {
        destination[i] = source[i];
    }
}

bool fpu_init(void) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    sampled_mxcsr_mask=0;
    sampled_mxcsr_mask_valid=false;
    available=false;
#ifdef SRVROS_FPU_INIT_HOST_TEST
    fpu_init_test_cpuid(1,&eax,&ebx,&ecx,&edx);
#else
    cpuid(1, &eax, &ebx, &ecx, &edx);
#endif
    (void)eax;
    (void)ebx;
    (void)ecx;

    if ((edx & (CPUID_FEATURE_FXSR | CPUID_FEATURE_SSE | CPUID_FEATURE_SSE2)) !=
        (CPUID_FEATURE_FXSR | CPUID_FEATURE_SSE | CPUID_FEATURE_SSE2)) {
        console_write("fpu: fxsave/sse/sse2 unavailable\n");
        available = false;
        return false;
    }

#ifdef SRVROS_FPU_INIT_HOST_TEST
    uint64_t cr0 = fpu_init_test_read_cr0();
#else
    uint64_t cr0 = read_cr0();
#endif
    cr0 &= ~(CR0_EM | CR0_TS);
    cr0 |= CR0_MP | CR0_NE;
#ifdef SRVROS_FPU_INIT_HOST_TEST
    fpu_init_test_write_cr0(cr0);
    uint64_t cr4 = fpu_init_test_read_cr4();
#else
    write_cr0(cr0);
    uint64_t cr4 = read_cr4();
#endif
    cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;
#ifdef SRVROS_FPU_INIT_HOST_TEST
    fpu_init_test_write_cr4(cr4);
#else
    write_cr4(cr4);
#endif
    for (size_t i=0;i<sizeof(initial_fxsave_area);i++) initial_fxsave_area[i]=0;
#ifdef SRVROS_FPU_INIT_HOST_TEST
    fpu_init_test_capture(initial_fxsave_area);
#else
    __asm__ volatile ("fninit" : : : "memory");
    uint32_t mxcsr = 0x1f80;
    __asm__ volatile ("ldmxcsr %0" : : "m"(mxcsr) : "memory");
    fxsave(initial_fxsave_area);
#endif
    uint32_t mask=(uint32_t)initial_fxsave_area[28] |
        ((uint32_t)initial_fxsave_area[29]<<8) | ((uint32_t)initial_fxsave_area[30]<<16) |
        ((uint32_t)initial_fxsave_area[31]<<24);
    sampled_mxcsr_mask=mask ? mask : 0xffbf;
    sampled_mxcsr_mask_valid=true;
    available = true;
    console_write("fpu: fxsave/sse/sse2 enabled\n");
    return true;
}

bool fpu_available(void) {
    return available;
}

void fpu_init_state(struct fpu_state *state) {
    if (state == NULL) {
        return;
    }
    state->active_user_trap = NULL;
    if (!available) {
        for (uint64_t i = 0; i < FPU_FXSAVE_AREA_SIZE; i++) {
            state->fxsave_area[i] = 0;
        }
        state->initialized = false;
        return;
    }
    copy_fx_area(state->fxsave_area, initial_fxsave_area);
    state->initialized = true;
}

void fpu_invalidate_user_trap(struct fpu_state *state) {
    if (state) state->active_user_trap = NULL;
}

void fpu_save(struct fpu_state *state) {
    if (!available || state == NULL) {
        return;
    }
    if (!state->initialized) {
        fpu_init_state(state);
    }
    fxsave(state->fxsave_area);
}

void fpu_restore(struct fpu_state *state) {
    if (!available || state == NULL) {
        return;
    }
    if (!state->initialized) {
        fpu_init_state(state);
    }
    fxrstor(state->fxsave_area);
}

void fpu_set_current_kernel_state(struct fpu_state *state) {
    current_kernel_state = state;
    user_state_loaded = false;
    if (available && state != NULL) {
        fpu_restore(state);
    }
}

void fpu_set_current_user_state(struct fpu_state *state) {
    current_user_state = state;
}

void fpu_switch_kernel_state(struct fpu_state *old_state,
    struct fpu_state *new_state,
    struct fpu_state *new_user_state) {
    if (available && old_state != NULL) {
        fpu_save(old_state);
    }
    current_kernel_state = new_state;
    current_user_state = new_user_state;
    user_state_loaded = false;
    if (available && new_state != NULL) {
        fpu_restore(new_state);
    }
}

void fpu_trap_enter(uint64_t cs, uint8_t *kernel_trap_area) {
    if (!available) return;
    if ((cs & 3) != 3) {
        fxsave(kernel_trap_area);
        return;
    }
    if (!user_state_loaded) {
        return;
    }
    if (current_user_state != NULL) {
        fpu_save(current_user_state);
    }
    if (current_kernel_state != NULL) {
        fpu_restore(current_kernel_state);
    }
    user_state_loaded = false;
}

void fpu_trap_exit(uint64_t cs, const uint8_t *kernel_trap_area) {
    if (!available) return;
    if ((cs & 3) != 3) {
        fxrstor(kernel_trap_area);
        return;
    }
    if (current_user_state == NULL) {
        return;
    }
    if (current_kernel_state != NULL) {
        fpu_save(current_kernel_state);
    }
    fpu_restore(current_user_state);
    user_state_loaded = true;
}

void fpu_enter_user(void) {
    if (!available || current_user_state == NULL) {
        return;
    }
    if (current_kernel_state != NULL) {
        fpu_save(current_kernel_state);
    }
    fpu_restore(current_user_state);
    user_state_loaded = true;
}

/* Private common-trap witnesses. This translation unit must be built without
 * SIMD and without instrumentation that can run before the FXSAVE boundary. */
#include "fpu_trap.h"
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/vmm.h>

#define FPU_PROOF_INVALID 0u
#define FPU_PROOF_VALID 1u
#define FPU_PROOF_CONSUMED 2u

/* Authority restriction only: do not change inherited CPU policy. These
 * integer-only reads must inline before the actual user FXSAVE boundary. */
static inline __attribute__((always_inline)) bool proof_policy_complete(void) {
#ifdef SRVROS_FPU_TRAP_HOST_TEST
    uint64_t cr4=fpu_test_read_cr4();
    uint64_t efer=fpu_test_read_efer();
#else
    uint64_t cr4;
    uint32_t lo,hi;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xc0000080u));
    uint64_t efer=((uint64_t)hi<<32)|lo;
#endif
    return (cr4 & CR4_OSFXSR) && !(cr4 & (1ull<<18)) && !(efer & (1ull<<14));
}

void fpu_common_trap_enter(const struct isr_frame *frame, uint8_t *area,
    struct fpu_user_trap_proof *proof) {
    uintptr_t aligned=(uintptr_t)frame & ~(uintptr_t)15;
    if (aligned < sizeof(*proof) || (uintptr_t)proof != aligned-sizeof(*proof)) {
        fpu_trap_enter(frame->cs,area);
        return;
    }
    proof->state=FPU_PROOF_INVALID;
    uint64_t cs=frame->cs;
    if ((cs & 3) != 3) {
        fpu_trap_enter(cs,area);
        return;
    }
    struct fpu_state *user=current_user_state, *kernel=current_kernel_state;
    bool saved=available && user_state_loaded && user && kernel && proof_policy_complete() &&
        user->initialized && kernel->initialized;
    fpu_trap_enter(cs,area); /* Actual user save precedes ordinary C queries. */
    if (!saved || user_state_loaded || current_user_state!=user || current_kernel_state!=kernel)
        return;
    struct scheduler_user_return_observation obs;
    if (!scheduler_observe_user_entry(&obs) || obs.user_fpu!=user || obs.kernel_fpu!=kernel ||
        current_user_state!=user || current_kernel_state!=kernel || user_state_loaded ||
        kernel->active_user_trap) return;
    proof->frame=frame;proof->thread_identity=obs.thread_identity;
    proof->address_space=obs.address_space;proof->image_generation=obs.image_generation;
    proof->user_fpu=user;proof->kernel_fpu=kernel;proof->process=obs.process;
    proof->state=FPU_PROOF_VALID;
    kernel->active_user_trap=proof;
}

void fpu_common_trap_exit(const struct isr_frame *frame, const uint8_t *area,
    struct fpu_user_trap_proof *proof) {
    uint64_t cs=frame->cs;
    if ((cs & 3)==3 && current_kernel_state && current_kernel_state->active_user_trap==proof)
        current_kernel_state->active_user_trap=NULL;
    /* No ordinary C call may follow this restore. */
    fpu_trap_exit(cs,area);
}

static bool proof_current(const struct isr_frame *frame, struct fpu_user_trap_proof **result) {
    if (!frame || (frame->cs & 3)!=3 || !available || user_state_loaded ||
        !current_kernel_state || !current_user_state || !proof_policy_complete()) return false;
    struct scheduler_user_return_observation obs;
    if (!scheduler_observe_user_return(&obs) || obs.kernel_fpu!=current_kernel_state ||
        obs.user_fpu!=current_user_state) return false;
    uintptr_t aligned=(uintptr_t)frame & ~(uintptr_t)15;
    if (aligned < sizeof(struct fpu_user_trap_proof)) return false;
    struct fpu_user_trap_proof *proof=(void *)(aligned-sizeof(*proof));
    if (obs.kernel_fpu->active_user_trap != proof) return false;
    if (proof->state!=FPU_PROOF_VALID || proof->frame!=frame ||
        proof->kernel_fpu!=obs.kernel_fpu || proof->user_fpu!=obs.user_fpu ||
        obs.thread_identity!=proof->thread_identity || obs.process!=proof->process ||
        obs.address_space!=proof->address_space || obs.image_generation!=proof->image_generation) return false;
    struct process_user_binding binding;
    if (!process_resolve_user_binding(obs.process,obs.user_thread,obs.image_generation,
            obs.address_space,&binding) || binding.user_fpu!=obs.user_fpu ||
        binding.fs_base!=obs.user_fs_base ||
        (obs.user_thread ? binding.kernel_stack_top!=0 : binding.kernel_stack_top!=obs.kernel_stack_top) ||
        !obs.user_fpu->initialized || !obs.kernel_fpu->initialized ||
        !vmm_prepare_user_kernel_mappings(obs.address_space)) return false;
    *result=proof;
    return true;
}

static bool kernel_extent(uintptr_t start, size_t length) {
    if (!length || start > UINTPTR_MAX-(length-1)) return false;
#ifdef SRVROS_FPU_TRAP_HOST_TEST
    uintptr_t first=fpu_test_kernel_address(start);
    uintptr_t last=fpu_test_kernel_address(start+length-1);
    return (first >> 47)==0x1ffff && (last >> 47)==0x1ffff;
#else
    return (start >> 47)==0x1ffff && ((start+length-1) >> 47)==0x1ffff;
#endif
}
static bool ranges_overlap(uintptr_t a, size_t alen, uintptr_t b, size_t blen) {
    return a <= b ? b-a < alen : a-b < blen;
}

/* Extent and numeric alias checks do not authenticate allocation/provenance. */
static bool private_object_range(const struct isr_frame *frame,
    const struct fpu_user_trap_proof *proof,const void *object,size_t length) {
    if (!object || ((uintptr_t)object & 15) || !kernel_extent((uintptr_t)object,length)) return false;
    uintptr_t aligned=(uintptr_t)frame & ~(uintptr_t)15;
    if (aligned < FPU_COMMON_RESERVATION || (uintptr_t)frame > UINTPTR_MAX-sizeof(*frame)) return false;
    uintptr_t start=aligned-FPU_COMMON_RESERVATION;
    size_t size=(uintptr_t)frame+sizeof(*frame)-start;
    return !ranges_overlap((uintptr_t)object,length,start,size) &&
        !ranges_overlap((uintptr_t)object,length,(uintptr_t)proof->user_fpu,sizeof(*proof->user_fpu)) &&
        !ranges_overlap((uintptr_t)object,length,(uintptr_t)proof->kernel_fpu,sizeof(*proof->kernel_fpu));
}
static bool private_snapshot_range(const struct isr_frame *frame,
    const struct fpu_user_trap_proof *proof,const struct fpu_user_snapshot *object) {
    return private_object_range(frame,proof,object,sizeof(*object));
}
static bool saved_mxcsr_valid(const struct fpu_user_snapshot *saved) {
    uint32_t mxcsr=(uint32_t)saved->bytes[24] | ((uint32_t)saved->bytes[25]<<8) |
        ((uint32_t)saved->bytes[26]<<16) | ((uint32_t)saved->bytes[27]<<24);
    return !(mxcsr & 0xffff0000u) && !(mxcsr & ~sampled_mxcsr_mask);
}

bool fpu_snapshot_user_trap(const struct isr_frame *frame, struct fpu_user_snapshot *out) {
    struct fpu_user_trap_proof *proof;
    if (!out || ((uintptr_t)out & 15) || !kernel_extent((uintptr_t)out,sizeof(*out)) ||
        !proof_current(frame,&proof) || !private_snapshot_range(frame,proof,out)) return false;
    copy_fx_area(out->bytes,proof->user_fpu->fxsave_area);
    return true;
}

bool fpu_restore_saved_user_trap(const struct isr_frame *frame, const struct fpu_user_snapshot *saved) {
    struct fpu_user_trap_proof *proof;
    if (!proof_current(frame,&proof) || !sampled_mxcsr_mask_valid ||
        !private_snapshot_range(frame,proof,saved)) return false;
    if (!saved_mxcsr_valid(saved)) return false;
    copy_fx_area(proof->user_fpu->fxsave_area,saved->bytes);
    proof->state=FPU_PROOF_CONSUMED;
    return true;
}

bool fpu_consume_user_trap(const struct isr_frame *frame) {
    struct fpu_user_trap_proof *proof;
    if (!proof_current(frame,&proof)) return false;
    proof->state=FPU_PROOF_CONSUMED;
    return true;
}

bool fpu_validate_user_trap(const struct isr_frame *frame) {
    struct fpu_user_trap_proof *proof;
    return proof_current(frame,&proof);
}

/* Validation produces a borrowed, single-use private commit; it is not a
 * capability and does not make mutable caller storage safe to retain. */
bool fpu_prepare_user_trap_commit(const struct isr_frame *frame,
    const struct fpu_user_snapshot *restore,struct fpu_user_trap_commit *out) {
    struct fpu_user_trap_proof *proof;
    if (!out || ((uintptr_t)out & 15) || !kernel_extent((uintptr_t)out,sizeof(*out)) ||
        !proof_current(frame,&proof) || !private_object_range(frame,proof,out,sizeof(*out))) return false;
    if (restore && (!sampled_mxcsr_mask_valid || !private_snapshot_range(frame,proof,restore) ||
        ranges_overlap((uintptr_t)out,sizeof(*out),(uintptr_t)restore,sizeof(*restore)) ||
        !saved_mxcsr_valid(restore))) return false;
    *out=(struct fpu_user_trap_commit){.proof=proof,.user_fpu=proof->user_fpu,.restore=restore};
    return true;
}

void fpu_commit_user_trap_prepared(const struct fpu_user_trap_commit *prepared) {
    if (prepared->restore) copy_fx_area(prepared->user_fpu->fxsave_area,prepared->restore->bytes);
    prepared->proof->state=FPU_PROOF_CONSUMED;
}
