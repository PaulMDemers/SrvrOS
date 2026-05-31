#include <stdio.h>
#include <stdint.h>

static __thread uint64_t tls_counter = 0x123456789abcdef0ull;
static __thread char tls_message[] = "srvros tls";

int main(void) {
    tls_counter += 0x10;
    tls_message[7] = 'T';
    tls_message[8] = 'L';
    tls_message[9] = 'S';

    printf("tlsprobe: counter=0x%llx message=%s\n",
        (unsigned long long)tls_counter,
        tls_message);

    if (tls_counter != 0x123456789abcdf00ull) {
        printf("tlsprobe: counter mismatch\n");
        return 1;
    }
    if (tls_message[7] != 'T' || tls_message[8] != 'L' || tls_message[9] != 'S') {
        printf("tlsprobe: string mismatch\n");
        return 1;
    }

    printf("tlsprobe: ok\n");
    return 0;
}
