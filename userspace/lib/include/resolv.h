#ifndef SRVROS_POSIX_RESOLV_H
#define SRVROS_POSIX_RESOLV_H

#include <arpa/nameser.h>
#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Query an absolute Internet-class name via IPv4 UDP. A successful return is
 * the complete wire reply size. The buffer is written only when it can hold the
 * entire reply. On failure returns -1 and sets the calling thread's h_errno.
 * Replies marked truncated fail with TRY_AGAIN and EMSGSIZE. */
int res_query(const char *name, int klass, int type, unsigned char *answer, int answer_size);

/* Expand a DNS wire name within [message, end). Return the encoded bytes
 * consumed at source, or -1 without modifying the output. The root is empty. */
int dn_expand(const unsigned char *message, const unsigned char *end,
    const unsigned char *source, char *name, int name_size);

#ifdef __cplusplus
}
#endif

#endif
