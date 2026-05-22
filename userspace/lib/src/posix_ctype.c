#include <ctype.h>

int isdigit(int c) {
    return c >= '0' && c <= '9';
}

int islower(int c) {
    return c >= 'a' && c <= 'z';
}

int isupper(int c) {
    return c >= 'A' && c <= 'Z';
}

int isalpha(int c) {
    return islower(c) || isupper(c);
}

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int iscntrl(int c) {
    return (c >= 0 && c < 32) || c == 127;
}

int isprint(int c) {
    return c >= 32 && c < 127;
}

int isgraph(int c) {
    return c > 32 && c < 127;
}

int isspace(int c) {
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

int isblank(int c) {
    return c == ' ' || c == '\t';
}

int isxdigit(int c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int ispunct(int c) {
    return isgraph(c) && !isalnum(c);
}

int tolower(int c) {
    return isupper(c) ? c + ('a' - 'A') : c;
}

int toupper(int c) {
    return islower(c) ? c - ('a' - 'A') : c;
}

int isalnum_l(int c, locale_t locale) { (void)locale; return isalnum(c); }
int isalpha_l(int c, locale_t locale) { (void)locale; return isalpha(c); }
int isblank_l(int c, locale_t locale) { (void)locale; return isblank(c); }
int iscntrl_l(int c, locale_t locale) { (void)locale; return iscntrl(c); }
int isdigit_l(int c, locale_t locale) { (void)locale; return isdigit(c); }
int isgraph_l(int c, locale_t locale) { (void)locale; return isgraph(c); }
int islower_l(int c, locale_t locale) { (void)locale; return islower(c); }
int isprint_l(int c, locale_t locale) { (void)locale; return isprint(c); }
int ispunct_l(int c, locale_t locale) { (void)locale; return ispunct(c); }
int isspace_l(int c, locale_t locale) { (void)locale; return isspace(c); }
int isupper_l(int c, locale_t locale) { (void)locale; return isupper(c); }
int isxdigit_l(int c, locale_t locale) { (void)locale; return isxdigit(c); }
int tolower_l(int c, locale_t locale) { (void)locale; return tolower(c); }
int toupper_l(int c, locale_t locale) { (void)locale; return toupper(c); }
