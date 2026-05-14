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
    cpuid(1, &eax, &ebx, &ecx, &edx);
    (void)eax;
    (void)ebx;
    (void)ecx;

    if ((edx & (CPUID_FEATURE_FXSR | CPUID_FEATURE_SSE | CPUID_FEATURE_SSE2)) !=
        (CPUID_FEATURE_FXSR | CPUID_FEATURE_SSE | CPUID_FEATURE_SSE2)) {
        console_write("fpu: fxsave/sse/sse2 unavailable\n");
        available = false;
        return false;
    }

    uint64_t cr0 = read_cr0();
    cr0 &= ~(CR0_EM | CR0_TS);
    cr0 |= CR0_MP | CR0_NE;
    write_cr0(cr0);

    uint64_t cr4 = read_cr4();
    cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;
    write_cr4(cr4);

    __asm__ volatile ("fninit" : : : "memory");
    uint32_t mxcsr = 0x1f80;
    __asm__ volatile ("ldmxcsr %0" : : "m"(mxcsr) : "memory");
    fxsave(initial_fxsave_area);
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

void fpu_trap_enter(uint64_t cs) {
    if (!available || (cs & 3) != 3 || !user_state_loaded) {
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

void fpu_trap_exit(uint64_t cs) {
    if (!available || (cs & 3) != 3 || current_user_state == NULL) {
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
