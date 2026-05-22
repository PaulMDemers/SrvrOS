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

locale_t newlocale(int category_mask, const char *locale, locale_t base) {
    (void)category_mask;
    (void)base;
    if (setlocale(LC_ALL, locale) == 0) {
        return 0;
    }
    return (locale_t)&c_locale;
}

void freelocale(locale_t locale) {
    (void)locale;
}

locale_t uselocale(locale_t locale) {
    (void)locale;
    return LC_GLOBAL_LOCALE;
}
