#include <locale.h>

static struct lconv c_locale = {
    .decimal_point = ".",
};

char *setlocale(int category, const char *locale) {
    (void)category;
    if (locale != 0 && locale[0] != '\0' && !(locale[0] == 'C' && locale[1] == '\0')) {
        return 0;
    }
    return "C";
}

struct lconv *localeconv(void) {
    return &c_locale;
}
