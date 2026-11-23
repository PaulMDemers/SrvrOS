#include <glib-object.h>
#include <stdio.h>

static void object_notified(GObject *object, GParamSpec *property, gpointer observed) {
    (void)object;
    *(const char **)observed = g_param_spec_get_name(property);
}

static gboolean dispatch(gpointer object) {
    g_object_notify_by_pspec(object, g_object_class_find_property(G_OBJECT_GET_CLASS(object), "enabled"));
    return G_SOURCE_REMOVE;
}

typedef struct { GObject parent; } ProbeObject;
typedef struct { GObjectClass parent; } ProbeObjectClass;
G_DEFINE_TYPE(ProbeObject, probe_object, G_TYPE_OBJECT)

static void probe_object_get_property(GObject *object, guint id, GValue *value, GParamSpec *property) {
    if (id == 1) g_value_set_boolean(value, TRUE);
    else G_OBJECT_WARN_INVALID_PROPERTY_ID(object, id, property);
}

static void probe_object_class_init(ProbeObjectClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->get_property = probe_object_get_property;
    g_object_class_install_property(object_class, 1,
        g_param_spec_boolean("enabled", NULL, NULL, TRUE, G_PARAM_READABLE));
}

static void probe_object_init(ProbeObject *object) { (void)object; }

int main(void) {
    GObject *object = g_object_new(probe_object_get_type(), NULL);
    const char *observed = NULL;
    gboolean enabled = FALSE;
    g_object_get(object, "enabled", &enabled, NULL);
    g_signal_connect(object, "notify::enabled", G_CALLBACK(object_notified), &observed);
    GMainContext *context = g_main_context_new();
    GSource *idle = g_idle_source_new();
    g_source_set_callback(idle, dispatch, object, NULL);
    g_source_attach(idle, context);
    gboolean dispatched = g_main_context_iteration(context, FALSE);
    g_source_unref(idle);
    g_main_context_unref(context);
    gpointer weak = object;
    g_object_add_weak_pointer(object, &weak);
    g_object_unref(object);
    if (!enabled || !dispatched || g_strcmp0(observed, "enabled") != 0 || weak != NULL) {
        fputs("glib-linked-gobject: property, main context or lifetime check failed\n", stderr);
        return 1;
    }
    puts("glib-linked-gobject: ok");
    return 0;
}
