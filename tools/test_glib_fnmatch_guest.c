/* C-locale fnmatch prerequisites for GIO's XDG MIME glob matcher. */
#include <fnmatch.h>
#include <stdio.h>
#include <string.h>

struct match_case {
    const char *pattern;
    const char *name;
    int flags;
    int expected;
};

#define YES 0
#define NO FNM_NOMATCH

static const struct match_case cases[] = {
    /* Complex MIME globs use flags zero; simple suffixes have a GIO fast path. */
    {"*.tar.?z", "archive.tar.xz", 0, YES},
    {"*.tar.?z", "archive.tar.gz2", 0, NO},
    {"image[0-9]*.png", "image7-large.png", 0, YES},
    {"image[0-9]*.png", "imagex.png", 0, NO},
    {"a*b*c", "axybzzc", 0, YES},
    {"a*b*c", "axybz", 0, NO},
    {"*", ".hidden/path", 0, YES},
    {"", "", 0, YES},
    {"", "x", 0, NO},
    {"**", "", 0, YES},
    {"**a*", "", 0, NO},
    /* Sets, literal closing bracket and hyphen, escapes, and C-locale classes. */
    {"[!a-c]", "z", 0, YES},
    {"[!a-c]", "b", 0, NO},
    {"[]a]", "]", 0, YES},
    {"[-a]", "-", 0, YES},
    {"[a-]", "-", 0, YES},
    {"[\\]]", "]", 0, YES},
    {"[\\-]", "-", 0, YES},
    {"[[:digit:]]", "7", 0, YES},
    {"[[:digit:]]", "x", 0, NO},
    {"[[:upper:]]", "q", FNM_CASEFOLD, YES},
    {"[![:unknown:]]", "q", 0, NO},
    {"[[.x.]]", "x", 0, YES},
    {"[[=x=]]", "x", 0, YES},
    {"[[.a.]-[.c.]]", "b", 0, YES},
    {"[[.ch.]]", "c", 0, NO},
    {"[[=ch=]]", "c", 0, NO},
    {"[abc", "[abc", 0, YES},
    {"abc\\", "abc", 0, NO},
    {"a\\*b", "a*b", 0, YES},
    {"a\\*b", "a\\xyzb", FNM_NOESCAPE, YES},
    {"a\\*b", "a*b", FNM_NOESCAPE, NO},
    /* A slash is ordinary with flags zero and protected by PATHNAME. */
    {"a*b", "a/x/b", 0, YES},
    {"a*b", "a/x/b", FNM_PATHNAME, NO},
    {"a?b", "a/b", FNM_PATHNAME, NO},
    {"a[/]b", "a/b", FNM_PATHNAME, NO},
    {"a\\/b", "a/b", FNM_PATHNAME, YES},
    {"a/*/b", "a/x/b", FNM_PATHNAME, YES},
    {"*", ".hidden", FNM_PERIOD, NO},
    {".*", ".hidden", FNM_PERIOD, YES},
    {"a/*", "a/.hidden", FNM_PERIOD | FNM_PATHNAME, NO},
    {"a/.*", "a/.hidden", FNM_PERIOD | FNM_PATHNAME, YES},
    {"a/*", "a/.hidden", FNM_PERIOD, YES},
    {"[.]hidden", ".hidden", FNM_PERIOD, NO},
    {"*.hidden", ".hidden", FNM_PERIOD, NO},
    {"\\.hidden", ".hidden", FNM_PERIOD, YES},
    {"*.TXT", "report.txt", FNM_CASEFOLD, YES},
    {"[A-C]", "b", FNM_CASEFOLD, YES},
    {"[A-z]", "_", FNM_CASEFOLD, YES},
    {"*.TXT", "report.txt", 0, NO},
    {"DIR/*.TXT", "dir/FILE.txt", FNM_CASEFOLD | FNM_PATHNAME, YES},
    {"DIR/*", "dir/.hidden", FNM_CASEFOLD | FNM_PATHNAME | FNM_PERIOD, NO},
    {"DIR/.*", "dir/.hidden", FNM_CASEFOLD | FNM_PATHNAME | FNM_PERIOD, YES},
    /* C locale: non-ASCII bytes are distinct and '?' consumes one byte. */
    {"?", "\xc3\xa9", 0, NO},
    {"??", "\xc3\xa9", 0, YES},
    {"\xc3\xa9", "\xc3\xa9", FNM_CASEFOLD, YES},
    {"\xc3\x89", "\xc3\xa9", FNM_CASEFOLD, NO},
    {"?", "\xff", 0, YES},
};

int main(void) {
    size_t i;
    char pattern[258], input[258];

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        int result = fnmatch(cases[i].pattern, cases[i].name, cases[i].flags);
        if (result != cases[i].expected) {
            printf("glib-fnmatch: case %zu failed (got %d expected %d)\n",
                   i, result, cases[i].expected);
            return 1;
        }
    }

    /* Many alternating stars exercise near misses without a huge allocation. */
    for (i = 0; i < 100; ++i) {
        pattern[2 * i] = '*';
        pattern[2 * i + 1] = 'a';
        input[i] = 'a';
    }
    pattern[200] = '*';
    pattern[201] = 'b';
    pattern[202] = '\0';
    input[100] = 'b';
    input[101] = '\0';
    if (fnmatch(pattern, input, 0) != YES) {
        printf("glib-fnmatch: repeated-star match failed\n");
        return 1;
    }
    input[100] = 'c';
    if (fnmatch(pattern, input, 0) != NO) {
        printf("glib-fnmatch: repeated-star near miss failed\n");
        return 1;
    }
    printf("glib-fnmatch: ok\n");
    return 0;
}
