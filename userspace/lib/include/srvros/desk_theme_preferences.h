#ifndef SRVROS_DESK_THEME_PREFERENCES_H
#define SRVROS_DESK_THEME_PREFERENCES_H
#include <srvros/desk_app.h>
#include <srvros/desk_widgets.h>
/* Resolve a window's last authoritative snapshot; before the first snapshot,
 * use the same Silver defaults as the desktop. Compact shared controls retain
 * their established 16px body font; apps configure document text separately.
 * No shared mutable theme. */
static inline struct desk_theme desk_app_theme(const struct desk_app *app) {
    struct desk_theme theme=desk_theme_silver;
    unsigned style=0,accent=0,glass=1;
    if(app && app->preferences_valid) {
        style=app->preferences.theme_style;accent=app->preferences.theme_accent;glass=app->preferences.glass_level;
    }
    (void)desk_theme_configure_style(&theme,style,accent,glass,16);
    return theme;
}
#endif
