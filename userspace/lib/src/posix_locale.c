#include <locale.h>

static struct lconv c_locale = {
    .decimal_point = ".",
    .thousands_sep = "",
    .grouping = "",
    .int_curr_symbol = "",
    .currency_symbol = "",
    .mon_decimal_point = "",
    .mon_thousands_sep = "",
    .mon_grouping = "",
    .positive_sign = "",
    .negative_sign = "",
    .int_frac_digits = 127,
    .frac_digits = 127,
    .p_cs_precedes = 127,
    .p_sep_by_space = 127,
    .n_cs_precedes = 127,
    .n_sep_by_space = 127,
    .p_sign_posn = 127,
    .n_sign_posn = 127,
    .int_p_cs_precedes = 127,
    .int_p_sep_by_space = 127,
    .int_n_cs_precedes = 127,
    .int_n_sep_by_space = 127,
    .int_p_sign_posn = 127,
    .int_n_sign_posn = 127,
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
