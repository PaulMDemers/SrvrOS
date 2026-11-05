#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>

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

namespace regex_constants {
enum error_type {
    error_collate = 0,
};
}

namespace _LIBCPP_ABI_NAMESPACE {
[[noreturn]] void __libcpp_verbose_abort(const char *format, ...) {
    if (format != nullptr) {
        va_list args;
        va_start(args, format);
        fputs("libc++ abort: ", stderr);
        vfprintf(stderr, format, args);
        fputc('\n', stderr);
        va_end(args);
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wframe-address"
    fprintf(stderr, "libc++ abort ra: %p %p %p\n",
        __builtin_return_address(0),
        __builtin_return_address(1),
        __builtin_return_address(2));
#pragma clang diagnostic pop
    abort();
}
}

class thread {
public:
    static unsigned hardware_concurrency() noexcept;
};

unsigned thread::hardware_concurrency() noexcept {
    return 1;
}

[[noreturn]] void __throw_bad_array_new_length() {
    abort();
}

[[noreturn]] void __throw_bad_cast() {
    abort();
}

[[noreturn]] void __throw_bad_function_call() {
    abort();
}

[[noreturn]] void __throw_invalid_argument(const char *) {
    abort();
}

[[noreturn]] void __throw_length_error(const char *) {
    abort();
}

[[noreturn]] void __throw_logic_error(const char *) {
    abort();
}

[[noreturn]] void __throw_out_of_range(const char *) {
    abort();
}

[[noreturn]] void __throw_out_of_range_fmt(const char *, ...) {
    abort();
}

[[noreturn]] void __throw_regex_error(regex_constants::error_type) {
    abort();
}

[[noreturn]] void __throw_system_error(int) {
    abort();
}

size_t _Hash_bytes(const void *data, size_t length, size_t seed) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t hash = seed != 0 ? seed : (size_t)1469598103934665603ull;
    for (size_t i = 0; i < length; i++) {
        hash ^= (size_t)bytes[i];
        hash *= (size_t)1099511628211ull;
    }
    return hash;
}
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

struct cxa_record { void (*destructor)(void *); void *arg; void *dso; };
static cxa_record cxa_records[128];
static size_t cxa_record_count;
static pthread_mutex_t cxa_lock = {};
static pthread_once_t cxa_once = PTHREAD_ONCE_INIT;
static int cxa_ready;
static void cxa_check(int result) { if (result) abort(); }
void __cxa_finalize(void *dso_handle);
static void cxx_tls_drain(void *);
static int cxx_tls_ready;
static void cxa_exit(void) { if (cxx_tls_ready) cxx_tls_drain(nullptr); __cxa_finalize(nullptr); }
static void cxa_init(void) { cxa_ready = (atexit(cxa_exit) == 0); }
int __cxa_atexit(void (*destructor)(void *), void *arg, void *dso_handle) {
    if (!destructor || pthread_once(&cxa_once, cxa_init) || !cxa_ready) return -1;
    cxa_check(pthread_mutex_lock(&cxa_lock));
    if (cxa_record_count == sizeof(cxa_records) / sizeof(cxa_records[0])) {
        cxa_check(pthread_mutex_unlock(&cxa_lock)); return -1;
    }
    cxa_records[cxa_record_count++] = {destructor, arg, dso_handle};
    cxa_check(pthread_mutex_unlock(&cxa_lock));
    return 0;
}
void __cxa_finalize(void *dso_handle) {
    for (;;) {
        cxa_check(pthread_mutex_lock(&cxa_lock));
        size_t i = cxa_record_count;
        while (i && dso_handle && cxa_records[i-1].dso != dso_handle) --i;
        if (!i) { cxa_check(pthread_mutex_unlock(&cxa_lock)); return; }
        --i;
        cxa_record record = cxa_records[i];
        for (size_t j=i+1; j<cxa_record_count; ++j) cxa_records[j-1]=cxa_records[j];
        --cxa_record_count;
        cxa_check(pthread_mutex_unlock(&cxa_lock));
        record.destructor(record.arg);
    }
}

struct cxx_tls_record {
    void (*destructor)(void *);
    void *arg;
    cxx_tls_record *next;
};
static _Thread_local cxx_tls_record *cxx_tls_head;
static pthread_key_t cxx_tls_key;
static pthread_once_t cxx_tls_once = PTHREAD_ONCE_INIT;
static void cxx_tls_drain(void *) {
    // pthread clears a key before invoking its destructor. Registrations made
    // by destructors prepend to this same thread's list and are drained too.
    while (cxx_tls_head != nullptr) {
        cxx_tls_record *record = cxx_tls_head;
        cxx_tls_head = record->next;
        void (*destructor)(void *) = record->destructor;
        void *arg = record->arg;
        free(record);
        destructor(arg);
    }
    (void)pthread_setspecific(cxx_tls_key, nullptr);
}
static void cxx_tls_init(void) {
    if (pthread_key_create(&cxx_tls_key, cxx_tls_drain) != 0) return;
    cxx_tls_ready = 1;
}
int __cxa_thread_atexit(void (*destructor)(void *), void *arg, void *dso_handle) {
    (void)dso_handle;
    if (!destructor || pthread_once(&cxa_once, cxa_init) || !cxa_ready ||
        pthread_once(&cxx_tls_once, cxx_tls_init) || !cxx_tls_ready) return -1;
    cxx_tls_record *record = (cxx_tls_record *)malloc(sizeof(*record));
    if (!record) return -1;
    record->destructor = destructor; record->arg = arg; record->next = cxx_tls_head;
    if (pthread_setspecific(cxx_tls_key, record) != 0) { free(record); return -1; }
    cxx_tls_head = record;
    return 0;
}

static pthread_mutex_t cxx_guard_mutex = {};
static pthread_cond_t cxx_guard_cond = PTHREAD_COND_INITIALIZER;
int __cxa_guard_acquire(uint64_t *guard) {
    unsigned char *bytes = (unsigned char *)guard;
    if (__atomic_load_n(bytes, __ATOMIC_ACQUIRE)) return 0;
    cxa_check(pthread_mutex_lock(&cxx_guard_mutex));
    while (bytes[1] && !__atomic_load_n(bytes, __ATOMIC_ACQUIRE))
        cxa_check(pthread_cond_wait(&cxx_guard_cond, &cxx_guard_mutex));
    int acquire = !__atomic_load_n(bytes, __ATOMIC_ACQUIRE);
    if (acquire) bytes[1] = 1;
    cxa_check(pthread_mutex_unlock(&cxx_guard_mutex));
    return acquire;
}
void __cxa_guard_release(uint64_t *guard) {
    unsigned char *bytes = (unsigned char *)guard;
    cxa_check(pthread_mutex_lock(&cxx_guard_mutex));
    __atomic_store_n(bytes, 1, __ATOMIC_RELEASE);
    bytes[1] = 0;
    cxa_check(pthread_cond_broadcast(&cxx_guard_cond));
    cxa_check(pthread_mutex_unlock(&cxx_guard_mutex));
}
void __cxa_guard_abort(uint64_t *guard) {
    unsigned char *bytes = (unsigned char *)guard;
    cxa_check(pthread_mutex_lock(&cxx_guard_mutex));
    bytes[1] = 0;
    cxa_check(pthread_cond_broadcast(&cxx_guard_cond));
    cxa_check(pthread_mutex_unlock(&cxx_guard_mutex));
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
