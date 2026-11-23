/* Exercise proxy-libintl 0.5 STUB_ONLY semantics; this is not catalog lookup. */
#define G_INTL_STATIC_COMPILATION 1
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#define CHECK(phase, condition) do { \
    if (!(condition)) { \
        printf("glib-proxy-intl: failure phase=%s line=%d\n", (phase), __LINE__); \
        return 1; \
    } \
} while (0)

int main(void) {
    char singular[] = "one item";
    char plural[] = "many items";
    char contextual[] = "menu\004Open";
    char contextual_plural[] = "menu\004items";
    char domain[] = "fixture-domain";
    char codeset[] = "UTF-8";

    CHECK("default-domain", strcmp(textdomain(NULL), "messages") == 0);
    CHECK("gettext", gettext(singular) == singular);
    CHECK("dgettext", dgettext(domain, singular) == singular);
    CHECK("dcgettext", dcgettext(domain, contextual, LC_MESSAGES) == contextual);
    CHECK("context-passthrough", strcmp(dgettext(domain, contextual), "menu\004Open") == 0);

    CHECK("ngettext-one", ngettext(singular, plural, 1) == singular);
    CHECK("ngettext-zero", ngettext(singular, plural, 0) == plural);
    CHECK("ngettext-many", ngettext(singular, plural, 2) == plural);
    CHECK("dngettext-one", dngettext(domain, singular, plural, 1) == singular);
    CHECK("dngettext-many", dngettext(domain, singular, plural, 3) == plural);
    CHECK("dcngettext-one", dcngettext(domain, contextual, contextual_plural, 1, LC_MESSAGES) == contextual);
    CHECK("dcngettext-many", dcngettext(domain, contextual, contextual_plural, 2, LC_MESSAGES) == contextual_plural);

    CHECK("set-domain", strcmp(textdomain(domain), "fixture-domain") == 0);
    domain[0] = 'X'; /* The upstream stub duplicates the selected domain. */
    CHECK("query-domain", strcmp(textdomain(NULL), "fixture-domain") == 0);
    CHECK("empty-domain", strcmp(textdomain(""), "") == 0);
    CHECK("query-empty-domain", strcmp(textdomain(NULL), "") == 0);
    CHECK("replace-domain", strcmp(textdomain("second"), "second") == 0);
    CHECK("bind", strcmp(bindtextdomain("second", "/somewhere"), "/dummy") == 0);
    CHECK("bind-query", strcmp(bindtextdomain("second", NULL), "/dummy") == 0);
    CHECK("codeset", bind_textdomain_codeset("second", codeset) == codeset);
    CHECK("codeset-query", bind_textdomain_codeset("second", NULL) == NULL);
    puts("glib-proxy-intl: ok");
    return 0;
}
