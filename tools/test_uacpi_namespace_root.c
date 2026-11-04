/* Exercise the tracked dependency's public null-scope resolution behavior. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../ports/upstream/uacpi/source/namespace.c"

static unsigned locks;

uacpi_status uacpi_rw_lock_read(struct uacpi_rw_lock *lock)
{
    (void)lock;
    locks++;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_rw_unlock_read(struct uacpi_rw_lock *lock)
{
    (void)lock;
    assert(locks);
    locks--;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_log(uacpi_log_level level, const uacpi_char *format)
{
    (void)level;
    (void)format;
}

void uacpi_log(uacpi_log_level level, const uacpi_char *format, ...)
{
    (void)level;
    (void)format;
}

uacpi_size uacpi_strlen(const uacpi_char *s) { return strlen(s); }
struct uacpi_runtime_context g_uacpi_rt_ctx;

int main(void)
{
    uacpi_namespace_node *root = uacpi_namespace_root(), *result = (void *)1;
    uacpi_namespace_node child = {
        .parent = root, .name.text = {'T', 'E', 'S', 'T'}
    };
    root->child = &child;
    assert(uacpi_namespace_node_find(NULL, "TEST", &result) == UACPI_STATUS_OK && result == &child);
    assert(uacpi_namespace_node_resolve_from_aml_namepath(NULL, "TEST", &result) == UACPI_STATUS_OK && result == &child);
    assert(uacpi_namespace_node_resolve_from_aml_namepath(NULL, "MISS", &result) == UACPI_STATUS_NOT_FOUND);
    assert(uacpi_namespace_node_resolve_from_aml_namepath(&child, "TEST", &result) == UACPI_STATUS_OK && result == &child);
    assert(uacpi_namespace_node_resolve_from_aml_namepath(NULL, "\\MISS", &result) == UACPI_STATUS_NOT_FOUND);
    assert(uacpi_namespace_node_resolve_from_aml_namepath(NULL, "^TEST", &result) == UACPI_STATUS_INVALID_ARGUMENT);
    assert(!locks);
    puts("uacpi-namespace-root: PASS null scope, found/missing relative and absolute names, parent search and root boundary");
}
