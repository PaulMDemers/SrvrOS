#ifndef SRVROS_FILE_VERIFY_KERNEL_H
#define SRVROS_FILE_VERIFY_KERNEL_H
#include <stdbool.h>
#include <stddef.h>
#include <srvros/file_verify.h>

typedef bool (*file_verify_copy_fn)(void *context,void *destination,uint64_t offset,size_t length);
/* The namespace syscall owner holds ordinary filesystem admission throughout.
 * No shared process content/descriptor is acquired or evicted. */
int64_t file_verify(const char *path,const struct srv_file_verify_request *request,
    file_verify_copy_fn copy,void *context);
#endif
