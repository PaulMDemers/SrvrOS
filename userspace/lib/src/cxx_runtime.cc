#include <stddef.h>
#include <stdint.h>

extern "C" {
void abort(void) __attribute__((noreturn));
void free(void *ptr);
void *malloc(size_t size);
int posix_memalign(void **memptr, size_t alignment, size_t size);
}

namespace std {
struct nothrow_t {
    explicit nothrow_t() = default;
};

enum class align_val_t : size_t {};

[[gnu::used]] const nothrow_t nothrow;
}

static void *cxx_allocate(size_t size) {
    if (size == 0) {
        size = 1;
    }
    void *ptr = malloc(size);
    if (ptr == nullptr) {
        abort();
    }
    return ptr;
}

static void *cxx_allocate_nothrow(size_t size) {
    if (size == 0) {
        size = 1;
    }
    return malloc(size);
}

static void *cxx_allocate_aligned(size_t size, size_t alignment, int abort_on_fail) {
    if (size == 0) {
        size = 1;
    }
    void *ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        if (abort_on_fail) {
            abort();
        }
        return nullptr;
    }
    return ptr;
}

void *operator new(size_t size) {
    return cxx_allocate(size);
}

void *operator new[](size_t size) {
    return cxx_allocate(size);
}

void *operator new(size_t size, const std::nothrow_t &) noexcept {
    return cxx_allocate_nothrow(size);
}

void *operator new[](size_t size, const std::nothrow_t &) noexcept {
    return cxx_allocate_nothrow(size);
}

void *operator new(size_t size, std::align_val_t alignment) {
    return cxx_allocate_aligned(size, (size_t)alignment, 1);
}

void *operator new[](size_t size, std::align_val_t alignment) {
    return cxx_allocate_aligned(size, (size_t)alignment, 1);
}

void *operator new(size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return cxx_allocate_aligned(size, (size_t)alignment, 0);
}

void *operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return cxx_allocate_aligned(size, (size_t)alignment, 0);
}

void operator delete(void *ptr) noexcept {
    free(ptr);
}

void operator delete[](void *ptr) noexcept {
    free(ptr);
}

void operator delete(void *ptr, size_t) noexcept {
    free(ptr);
}

void operator delete[](void *ptr, size_t) noexcept {
    free(ptr);
}

void operator delete(void *ptr, std::align_val_t) noexcept {
    free(ptr);
}

void operator delete[](void *ptr, std::align_val_t) noexcept {
    free(ptr);
}

void operator delete(void *ptr, size_t, std::align_val_t) noexcept {
    free(ptr);
}

void operator delete[](void *ptr, size_t, std::align_val_t) noexcept {
    free(ptr);
}

extern "C" {

uintptr_t __stack_chk_guard = 0x736276726f73534bull;
unsigned char __libc_single_threaded;
void *__libc_stack_end;

void __stack_chk_fail(void) {
    abort();
}

void __cxa_pure_virtual(void) {
    abort();
}

void __cxa_deleted_virtual(void) {
    abort();
}

int __cxa_atexit(void (*destructor)(void *), void *arg, void *dso_handle) {
    (void)dso_handle;
    struct cxa_record {
        void (*destructor)(void *);
        void *arg;
    };
    static cxa_record records[128];
    static size_t record_count;
    if (destructor == nullptr || record_count >= sizeof(records) / sizeof(records[0])) {
        return -1;
    }
    records[record_count].destructor = destructor;
    records[record_count].arg = arg;
    record_count++;
    return 0;
}

int __cxa_thread_atexit(void (*destructor)(void *), void *arg, void *dso_handle) {
    return __cxa_atexit(destructor, arg, dso_handle);
}

void __cxa_finalize(void *dso_handle) {
    (void)dso_handle;
}

int __cxa_guard_acquire(uint64_t *guard) {
    unsigned char *byte = (unsigned char *)guard;
    return __atomic_load_n(byte, __ATOMIC_ACQUIRE) == 0;
}

void __cxa_guard_release(uint64_t *guard) {
    unsigned char *byte = (unsigned char *)guard;
    __atomic_store_n(byte, 1, __ATOMIC_RELEASE);
}

void __cxa_guard_abort(uint64_t *guard) {
    (void)guard;
}

void *__cxa_demangle(const char *mangled_name, char *output_buffer, size_t *length, int *status) {
    (void)mangled_name;
    (void)output_buffer;
    (void)length;
    if (status != nullptr) {
        *status = -2;
    }
    return nullptr;
}

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    (void)assertion;
    (void)file;
    (void)line;
    (void)function;
    abort();
}

int __popcountdi2(unsigned long long value) {
    int count = 0;
    while (value != 0) {
        count += (int)(value & 1ull);
        value >>= 1;
    }
    return count;
}

typedef unsigned __int128 srvros_tu_int;

srvros_tu_int __udivmodti4(srvros_tu_int numerator, srvros_tu_int denominator, srvros_tu_int *remainder) {
    if (denominator == 0) {
        abort();
    }
    srvros_tu_int quotient = 0;
    srvros_tu_int partial = 0;
    for (int bit = 127; bit >= 0; bit--) {
        partial = (partial << 1) | ((numerator >> bit) & 1u);
        if (partial >= denominator) {
            partial -= denominator;
            quotient |= ((srvros_tu_int)1u << bit);
        }
    }
    if (remainder != nullptr) {
        *remainder = partial;
    }
    return quotient;
}

srvros_tu_int __udivti3(srvros_tu_int numerator, srvros_tu_int denominator) {
    return __udivmodti4(numerator, denominator, nullptr);
}

srvros_tu_int __umodti3(srvros_tu_int numerator, srvros_tu_int denominator) {
    srvros_tu_int remainder = 0;
    (void)__udivmodti4(numerator, denominator, &remainder);
    return remainder;
}

}
