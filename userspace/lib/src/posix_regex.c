#include <regex.h>
#include <stdlib.h>
#include <string.h>

struct token {
    const char *start;
    const char *end;
    char quantifier;
};

static char fold_char(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static int same_char(char left, char right, int flags) {
    if ((flags & REG_ICASE) != 0) {
        left = fold_char(left);
        right = fold_char(right);
    }
    return left == right;
}

static int find_class_end(const char *pattern, const char **end_out) {
    const char *p = pattern + 1;
    if (*p == '^' || *p == '!') {
        p++;
    }
    if (*p == ']') {
        p++;
    }
    while (*p != '\0') {
        if (p[0] == '[' && p[1] == ':') {
            p += 2;
            while (p[0] != '\0' && !(p[0] == ':' && p[1] == ']')) {
                p++;
            }
            if (p[0] == ':' && p[1] == ']') {
                p += 2;
                continue;
            }
        }
        if (*p == '\\' && p[1] != '\0') {
            p += 2;
            continue;
        }
        if (*p == ']') {
            *end_out = p + 1;
            return 1;
        }
        p++;
    }
    return 0;
}

static int next_token(const char *pattern, int extended, struct token *token) {
    const char *p = pattern;
    token->start = p;
    token->quantifier = '\0';
    if (*p == '\0') {
        token->end = p;
        return 0;
    }
    if (*p == '[') {
        if (!find_class_end(p, &token->end)) {
            return -1;
        }
    } else if (*p == '\\' && p[1] != '\0') {
        token->end = p + 2;
    } else {
        token->end = p + 1;
    }
    if (*token->end == '*' || (extended && (*token->end == '+' || *token->end == '?'))) {
        token->quantifier = *token->end;
        token->end++;
    }
    return 1;
}

static int validate_pattern(const char *pattern, int extended) {
    const char *p = pattern;
    if (*p == '*' || (extended && (*p == '+' || *p == '?'))) {
        return REG_BADRPT;
    }
    while (*p != '\0') {
        struct token token;
        int result = next_token(p, extended, &token);
        if (result < 0) {
            return REG_EBRACK;
        }
        if (result == 0) {
            break;
        }
        if (extended && (*token.start == '(' || *token.start == ')' || *token.start == '|')) {
            return REG_BADPAT;
        }
        p = token.end;
        if (*p == '*' || (extended && (*p == '+' || *p == '?'))) {
            return REG_BADRPT;
        }
    }
    return REG_OK;
}

static int class_name_match(const char *name, size_t length, const char *literal) {
    return strlen(literal) == length && strncmp(name, literal, length) == 0;
}

static int match_named_class(const char *name, size_t length, char c) {
    if (class_name_match(name, length, "digit")) return c >= '0' && c <= '9';
    if (class_name_match(name, length, "lower")) return c >= 'a' && c <= 'z';
    if (class_name_match(name, length, "upper")) return c >= 'A' && c <= 'Z';
    if (class_name_match(name, length, "alpha")) return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    if (class_name_match(name, length, "alnum")) return match_named_class("alpha", 5, c) || match_named_class("digit", 5, c);
    if (class_name_match(name, length, "xdigit")) return (c >= '0' && c <= '9') || (fold_char(c) >= 'a' && fold_char(c) <= 'f');
    if (class_name_match(name, length, "space")) return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    if (class_name_match(name, length, "blank")) return c == ' ' || c == '\t';
    return 0;
}

static int class_contains(const char *start, const char *end, char c, int flags) {
    const char *p = start + 1;
    int invert = 0;
    int matched = 0;
    if (*p == '^' || *p == '!') {
        invert = 1;
        p++;
    }
    if (*p == ']') {
        if (same_char(c, ']', flags)) {
            matched = 1;
        }
        p++;
    }
    while (p < end - 1) {
        char left;
        if (p + 3 < end && p[0] == '[' && p[1] == ':') {
            const char *name = p + 2;
            const char *close = name;
            while (close + 1 < end && !(close[0] == ':' && close[1] == ']')) {
                close++;
            }
            if (close + 1 < end && match_named_class(name, (size_t)(close - name), c)) {
                matched = 1;
            }
            if (close + 1 < end) {
                p = close + 2;
                continue;
            }
        }
        left = *p == '\\' && p + 1 < end - 1 ? *++p : *p;
        if (p + 2 < end - 1 && p[1] == '-') {
            char right = p[2];
            if (right == '\\' && p + 3 < end - 1) {
                right = p[3];
                p++;
            }
            char fc = (flags & REG_ICASE) != 0 ? fold_char(c) : c;
            char fl = (flags & REG_ICASE) != 0 ? fold_char(left) : left;
            char fr = (flags & REG_ICASE) != 0 ? fold_char(right) : right;
            if (fc >= fl && fc <= fr) {
                matched = 1;
            }
            p += 3;
            continue;
        }
        if (same_char(c, left, flags)) {
            matched = 1;
        }
        p++;
    }
    return invert ? !matched : matched;
}

static int token_matches(const struct token *token, char c, int flags) {
    if (c == '\0') {
        return 0;
    }
    if (*token->start == '.') {
        return (flags & REG_NEWLINE) == 0 || c != '\n';
    }
    if (*token->start == '[') {
        return class_contains(token->start, token->end - (token->quantifier ? 1 : 0), c, flags);
    }
    if (*token->start == '\\' && token->start[1] != '\0') {
        return same_char(c, token->start[1], flags);
    }
    return same_char(c, *token->start, flags);
}

static const char *token_pattern_end(const struct token *token) {
    return token->quantifier == '\0' ? token->end : token->end - 1;
}

static int match_here(const char *pattern, const char *text, int cflags, int eflags, const char **end_out);

static int match_repeat(const struct token *token, const char *rest, const char *text,
    int min_count, int cflags, int eflags, const char **end_out) {
    const char *positions[128];
    size_t count = 0;
    positions[count++] = text;
    while (count < sizeof(positions) / sizeof(positions[0]) &&
        token_matches(token, *positions[count - 1], cflags)) {
        positions[count] = positions[count - 1] + 1;
        count++;
    }
    for (size_t i = count; i > 0; i--) {
        size_t used = i - 1;
        if ((int)used < min_count) {
            break;
        }
        if (match_here(rest, positions[used], cflags, eflags, end_out)) {
            return 1;
        }
    }
    return 0;
}

static int match_here(const char *pattern, const char *text, int cflags, int eflags, const char **end_out) {
    struct token token;
    int result;
    if (*pattern == '\0') {
        *end_out = text;
        return 1;
    }
    if (*pattern == '$' && pattern[1] == '\0') {
        if ((*text == '\0') || ((cflags & REG_NEWLINE) != 0 && *text == '\n' && (eflags & REG_NOTEOL) == 0)) {
            *end_out = text;
            return 1;
        }
        return 0;
    }
    result = next_token(pattern, (cflags & REG_EXTENDED) != 0, &token);
    if (result <= 0) {
        return 0;
    }
    if (token.quantifier == '*') {
        return match_repeat(&token, token.end, text, 0, cflags, eflags, end_out);
    }
    if (token.quantifier == '+') {
        return match_repeat(&token, token.end, text, 1, cflags, eflags, end_out);
    }
    if (token.quantifier == '?') {
        if (token_matches(&token, *text, cflags) &&
            match_here(token.end, text + 1, cflags, eflags, end_out)) {
            return 1;
        }
        return match_here(token.end, text, cflags, eflags, end_out);
    }
    if (!token_matches(&token, *text, cflags)) {
        return 0;
    }
    return match_here(token_pattern_end(&token), text + 1, cflags, eflags, end_out);
}

int regcomp(regex_t *preg, const char *regex, int cflags) {
    int error;
    if (preg == 0 || regex == 0) {
        return REG_BADPAT;
    }
    preg->pattern = 0;
    preg->cflags = cflags;
    preg->re_nsub = 0;
    error = validate_pattern(regex, (cflags & REG_EXTENDED) != 0);
    if (error != REG_OK) {
        return error;
    }
    preg->pattern = strdup(regex);
    if (preg->pattern == 0) {
        return REG_ESPACE;
    }
    return REG_OK;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch,
    regmatch_t pmatch[], int eflags) {
    const char *pattern;
    const char *text;
    const char *end = 0;
    if (preg == 0 || preg->pattern == 0 || string == 0) {
        return REG_NOMATCH;
    }
    pattern = preg->pattern;
    if (*pattern == '^') {
        if ((eflags & REG_NOTBOL) == 0 && match_here(pattern + 1, string, preg->cflags, eflags, &end)) {
            if (nmatch > 0 && pmatch != 0) {
                pmatch[0].rm_so = 0;
                pmatch[0].rm_eo = (regoff_t)(end - string);
            }
            return REG_OK;
        }
        return REG_NOMATCH;
    }
    for (text = string;; text++) {
        if (match_here(pattern, text, preg->cflags, eflags, &end)) {
            if (nmatch > 0 && pmatch != 0) {
                pmatch[0].rm_so = (regoff_t)(text - string);
                pmatch[0].rm_eo = (regoff_t)(end - string);
            }
            return REG_OK;
        }
        if (*text == '\0') {
            break;
        }
        if ((preg->cflags & REG_NEWLINE) != 0 && *text == '\n' && (eflags & REG_NOTBOL) != 0) {
            break;
        }
    }
    return REG_NOMATCH;
}

size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size) {
    const char *message = "regex error";
    (void)preg;
    switch (errcode) {
    case REG_OK: message = "success"; break;
    case REG_NOMATCH: message = "no match"; break;
    case REG_BADPAT: message = "bad pattern"; break;
    case REG_EESCAPE: message = "trailing escape"; break;
    case REG_EBRACK: message = "unmatched bracket"; break;
    case REG_BADRPT: message = "bad repeat"; break;
    case REG_ESPACE: message = "out of memory"; break;
    default: break;
    }
    size_t needed = strlen(message) + 1;
    if (errbuf != 0 && errbuf_size > 0) {
        size_t copy = needed < errbuf_size ? needed : errbuf_size;
        memcpy(errbuf, message, copy);
        errbuf[copy - 1] = '\0';
    }
    return needed;
}

void regfree(regex_t *preg) {
    if (preg != 0) {
        free(preg->pattern);
        preg->pattern = 0;
        preg->cflags = 0;
        preg->re_nsub = 0;
    }
}
