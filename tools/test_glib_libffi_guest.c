/* Exercise the target's libffi calling convention and advertised closures. */
#include <ffi.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(phase, condition) do { \
    if (!(condition)) { \
        printf("glib-libffi: failure phase=%s line=%d\n", (phase), __LINE__); \
        return 1; \
    } \
} while (0)

static double spilled_call(uint64_t a, uint64_t b, uint64_t c, uint64_t d,
    uint64_t e, uint64_t f, uint64_t g, uint64_t h, double value) {
    return (double)(a + b + c + d + e + f + g + h) + value;
}

static double variadic_call(int count, ...) {
    va_list args;
    va_start(args, count);
    int integer = va_arg(args, int);
    double real = va_arg(args, double);
    va_end(args);
    return count + integer + real;
}

struct pair { uint64_t number; double real; };
static struct pair aggregate_call(uint64_t value, double real) {
    struct pair result = { value + 11, real + 0.5 };
    return result;
}

#if FFI_CLOSURES
static void closure_callback(ffi_cif *cif, void *result, void **args, void *user) {
    (void)cif;
    uint64_t integer = *(uint64_t *)args[0];
    double real = *(double *)args[1];
    *(uint64_t *)result = integer + (uint64_t)(real * 4) + *(uint64_t *)user;
}

static int exercise_closure(ffi_abi abi) {
    ffi_cif cif;
    ffi_type *args[] = { &ffi_type_uint64, &ffi_type_double };
    CHECK("closure-prep", ffi_prep_cif(&cif, abi, 2, &ffi_type_uint64, args) == FFI_OK);
    void *entry = NULL;
    ffi_closure *closure = ffi_closure_alloc(sizeof(*closure), &entry);
    CHECK("closure-alloc", closure != NULL && entry != NULL && entry != (void *)closure);
    uint64_t userdata = 100;
    CHECK("closure-mismatched-entry",
        ffi_prep_closure_loc(closure, &cif, closure_callback, &userdata, closure) == FFI_BAD_ARGTYPE);
    CHECK("closure-bind",
        ffi_prep_closure_loc(closure, &cif, closure_callback, &userdata, entry) == FFI_OK);

    uint64_t actual;
    if (abi == FFI_WIN64) {
        uint64_t (__attribute__((ms_abi)) *callback)(uint64_t, double) = entry;
        actual = callback(7, 2.5);
    } else {
        uint64_t (*callback)(uint64_t, double) = entry;
        actual = callback(7, 2.5);
    }
    ffi_closure_free(closure);
    CHECK("closure-call", actual == 117);
    return 0;
}

struct published_callback {
    uint64_t (*entry)(uint64_t, double);
    uint64_t result;
};

static void *call_published(void *opaque) {
    struct published_callback *callback = opaque;
    callback->result = callback->entry(7, 2.5);
    return NULL;
}

static int exercise_publication(void) {
    ffi_cif cif;
    ffi_type *args[] = { &ffi_type_uint64, &ffi_type_double };
    CHECK("publish-prep", ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_uint64, args) == FFI_OK);
    void *entry = NULL;
    ffi_closure *closure = ffi_closure_alloc(sizeof(*closure), &entry);
    uint64_t userdata = 100;
    CHECK("publish-alloc", closure != NULL && entry != NULL);
    CHECK("publish-bind", ffi_prep_closure_loc(closure, &cif, closure_callback, &userdata, entry) == FFI_OK);
    struct published_callback callback = { .entry = entry };
    pthread_t thread;
    CHECK("publish-create", pthread_create(&thread, NULL, call_published, &callback) == 0);
    CHECK("publish-join", pthread_join(thread, NULL) == 0);
    ffi_closure_free(closure);
    CHECK("publish-result", callback.result == 117);
    return 0;
}

static void raw_callback(ffi_cif *cif, void *result, ffi_raw *args, void *user) {
    (void)cif;
    *(int64_t *)result = args[0].sint + *(int64_t *)user;
}

static int exercise_raw_closures(void) {
    ffi_cif cif;
    ffi_type *args[] = { &ffi_type_sint64 };
    int64_t userdata = 100;
    void *entry = NULL;
    CHECK("raw-prep", ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_sint64, args) == FFI_OK);
    ffi_raw_closure *raw = ffi_closure_alloc(sizeof(*raw), &entry);
    CHECK("raw-alloc", raw != NULL && entry != NULL);
    CHECK("raw-bind", ffi_prep_raw_closure(raw, &cif, raw_callback, &userdata) == FFI_OK);
    int64_t (*callback)(int64_t) = entry;
    int64_t result = callback(17);
    ffi_closure_free(raw);
    CHECK("raw-call", result == 117);

    ffi_java_raw_closure *java = ffi_closure_alloc(sizeof(*java), &entry);
    CHECK("java-raw-alloc", java != NULL && entry != NULL);
    /* Verify this exported compatibility entry point despite its deprecation. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    CHECK("java-raw-bind", ffi_prep_java_raw_closure(java, &cif, raw_callback, &userdata) == FFI_OK);
#pragma GCC diagnostic pop
    callback = entry;
    result = callback(17);
    ffi_closure_free(java);
    CHECK("java-raw-call", result == 117);
    return 0;
}

static void *closure_worker(void *unused) {
    (void)unused;
    return (void *)(uintptr_t)exercise_closure(FFI_DEFAULT_ABI);
}

static int exercise_slots(void) {
    ffi_closure *closures[128];
    void *entries[128];
    for (size_t i = 0; i < sizeof(closures) / sizeof(*closures); i++) {
        closures[i] = ffi_closure_alloc(sizeof(*closures[i]), &entries[i]);
        CHECK("slot-alloc", closures[i] != NULL && entries[i] != NULL);
        for (size_t j = 0; j < i; j++) {
            CHECK("slot-distinct", entries[i] != entries[j]);
        }
    }

    void *entry = entries[0];
    CHECK("slot-exhaustion", ffi_closure_alloc(sizeof(ffi_closure), &entry) == NULL && entry == NULL);
    ffi_closure_free(closures[0]);
    closures[0] = ffi_closure_alloc(sizeof(ffi_closure), &entry);
    CHECK("slot-reuse", closures[0] != NULL && entry == entries[0]);
    for (size_t i = 0; i < sizeof(closures) / sizeof(*closures); i++) {
        ffi_closure_free(closures[i]);
    }
    return 0;
}
#endif

int main(void) {
    ffi_cif cif;
    ffi_type *spill_types[] = { &ffi_type_uint64, &ffi_type_uint64,
        &ffi_type_uint64, &ffi_type_uint64, &ffi_type_uint64, &ffi_type_uint64,
        &ffi_type_uint64, &ffi_type_uint64, &ffi_type_double };
    uint64_t integers[] = {1, 2, 3, 4, 5, 6, 7, 8};
    double fraction = 0.5, real_result = 0;
    void *spill_values[] = { &integers[0], &integers[1], &integers[2], &integers[3],
        &integers[4], &integers[5], &integers[6], &integers[7], &fraction };
    CHECK("spill-prep", ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 9, &ffi_type_double, spill_types) == FFI_OK);
    ffi_call(&cif, FFI_FN(spilled_call), &real_result, spill_values);
    CHECK("spill-call", real_result == 36.5);

    ffi_type *var_types[] = { &ffi_type_sint, &ffi_type_sint, &ffi_type_double };
    int count = 2, integer = 7;
    double promoted = 2.5;
    void *var_values[] = { &count, &integer, &promoted };
    CHECK("varargs-prep", ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, 1, 3,
        &ffi_type_double, var_types) == FFI_OK);
    real_result = 0;
    ffi_call(&cif, FFI_FN(variadic_call), &real_result, var_values);
    CHECK("varargs-call", real_result == 11.5);

    ffi_type *pair_elements[] = { &ffi_type_uint64, &ffi_type_double, NULL };
    ffi_type pair_type = { 0, 0, FFI_TYPE_STRUCT, pair_elements };
    ffi_type *pair_args[] = { &ffi_type_uint64, &ffi_type_double };
    uint64_t value = 31;
    double input = 4.0;
    void *pair_values[] = { &value, &input };
    struct pair pair_result = {0, 0};
    CHECK("aggregate-prep", ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2,
        &pair_type, pair_args) == FFI_OK);
    ffi_call(&cif, FFI_FN(aggregate_call), &pair_result, pair_values);
    CHECK("aggregate-call", pair_result.number == 42 && pair_result.real == 4.5);

#if FFI_CLOSURES
    CHECK("win64-closure", exercise_closure(FFI_WIN64) == 0);
    CHECK("publication", exercise_publication() == 0);
    CHECK("raw-closures", exercise_raw_closures() == 0);
    CHECK("slots", exercise_slots() == 0);
    pthread_t threads[2];
    for (size_t i = 0; i < sizeof(threads) / sizeof(*threads); i++) {
        CHECK("worker-create", pthread_create(&threads[i], NULL, closure_worker, NULL) == 0);
    }
    for (size_t i = 0; i < sizeof(threads) / sizeof(*threads); i++) {
        void *result = NULL;
        CHECK("worker-join", pthread_join(threads[i], &result) == 0 && result == NULL);
    }
    puts("glib-libffi: ok");
#else
    puts("glib-libffi: call-only (closures not advertised)");
#endif
    return 0;
}
