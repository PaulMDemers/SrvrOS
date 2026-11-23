/* Real GResolver synchronous numeric-address handling, with no DNS backend. */
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

#define CHECK(phase, condition) do { if (!(condition)) { \
    printf("gio-resolver-address: failure %s line=%d\n", phase, __LINE__); \
    return 1; \
} } while (0)

typedef struct { GResolver parent; unsigned calls; } ProbeResolver;
typedef struct { GResolverClass parent; } ProbeResolverClass;
G_DEFINE_TYPE(ProbeResolver, probe_resolver, G_TYPE_RESOLVER)

static GList *probe_lookup(GResolver *resolver, const gchar *hostname,
                           GCancellable *cancellable, GError **error) {
    ProbeResolver *probe = (ProbeResolver *)resolver;
    static const guint8 bytes[4] = { 192, 0, 2, 123 };
    (void)cancellable;
    ++probe->calls;
    if (strcmp(hostname, "ordinary.example") != 0) {
        g_set_error_literal(error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND,
                            "unexpected hostname");
        return NULL;
    }
    return g_list_append(NULL, g_inet_address_new_from_bytes(bytes, G_SOCKET_FAMILY_IPV4));
}
static void probe_resolver_class_init(ProbeResolverClass *klass) {
    G_RESOLVER_CLASS(klass)->lookup_by_name = probe_lookup;
}
static void probe_resolver_init(ProbeResolver *self) { self->calls = 0; }

static int address_is(GList *list, const char *expected) {
    if (!list || list->next || !G_IS_INET_ADDRESS(list->data)) return 0;
    gchar *text = g_inet_address_to_string(G_INET_ADDRESS(list->data));
    int matches = text && strcmp(text, expected) == 0;
    g_free(text);
    return matches;
}

int main(void) {
    static const char *const rejected[] = {
        "127.1", "0x7f000001", "2130706433", "0177.0.0.1", "127.0.0.009"
    };
    ProbeResolver *probe = g_object_new(probe_resolver_get_type(), NULL);
    CHECK("new", probe != NULL);
    GError *error = NULL;
    GList *list = g_resolver_lookup_by_name(G_RESOLVER(probe), "192.0.2.123", NULL, &error);
    CHECK("canonical", error == NULL && address_is(list, "192.0.2.123") && probe->calls == 0);
    g_resolver_free_addresses(list);
    for (unsigned i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        error = NULL;
        list = g_resolver_lookup_by_name(G_RESOLVER(probe), rejected[i], NULL, &error);
        CHECK("legacy-rejected", list == NULL && error && error->domain == G_RESOLVER_ERROR &&
              error->code == G_RESOLVER_ERROR_NOT_FOUND && probe->calls == 0);
        g_clear_error(&error);
    }
    list = g_resolver_lookup_by_name(G_RESOLVER(probe), "ordinary.example", NULL, &error);
    CHECK("ordinary-backend", error == NULL && probe->calls == 1 && address_is(list, "192.0.2.123"));
    g_resolver_free_addresses(list);
    g_object_unref(probe);
    printf("gio-resolver-address: ok\n");
    return 0;
}
