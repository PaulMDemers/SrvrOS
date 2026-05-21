#include <regex.h>
#include <stdlib.h>
#include <string.h>

#define REGEX_MAX_MATCHES 16
#define REGEX_MAX_REPEAT 128

struct regex_context {
    const char *pattern_base;
    const char *string_base;
    int cflags;
    int eflags;
    size_t nmatch;
};

struct regex_state {
    const char *text;
    regmatch_t match[REGEX_MAX_MATCHES];
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

static void init_matches(regmatch_t *match, size_t count) {
    for (size_t i = 0; i < count; i++) {
        match[i].rm_so = -1;
        match[i].rm_eo = -1;
    }
}

static int in_class_expr(const char *p, const char *end) {
    return p + 1 < end && p[0] == '[' && p[1] == ':';
}

static int find_class_end(const char *pattern, const char *limit, const char **end_out) {
    const char *p = pattern + 1;
    if (p < limit && (*p == '^' || *p == '!')) {
        p++;
    }
    if (p < limit && *p == ']') {
        p++;
    }
    while (p < limit && *p != '\0') {
        if (in_class_expr(p, limit)) {
            p += 2;
            while (p + 1 < limit && !(p[0] == ':' && p[1] == ']')) {
                p++;
            }
            if (p + 1 < limit) {
                p += 2;
                continue;
            }
        }
        if (*p == '\\' && p + 1 < limit) {
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

static int find_group_end(const char *pattern, const char *limit, const char **end_out) {
    const char *p = pattern + 1;
    int depth = 1;
    while (p < limit && *p != '\0') {
        if (*p == '[') {
            const char *class_end = 0;
            if (!find_class_end(p, limit, &class_end)) {
                return 0;
            }
            p = class_end;
            continue;
        }
        if (*p == '\\' && p + 1 < limit) {
            p += 2;
            continue;
        }
        if (*p == '(') {
            depth++;
        } else if (*p == ')') {
            depth--;
            if (depth == 0) {
                *end_out = p + 1;
                return 1;
            }
        }
        p++;
    }
    return 0;
}

static int capture_index_for(const char *base, const char *target) {
    int index = 1;
    for (const char *p = base; p < target && *p != '\0'; p++) {
        if (*p == '[') {
            const char *class_end = 0;
            if (find_class_end(p, target, &class_end)) {
                p = class_end - 1;
            }
            continue;
        }
        if (*p == '\\' && p[1] != '\0') {
            p++;
            continue;
        }
        if (*p == '(') {
            index++;
        }
    }
    return index;
}

static const char *find_top_level_alt(const char *start, const char *end) {
    for (const char *p = start; p < end; p++) {
        if (*p == '[') {
            const char *class_end = 0;
            if (!find_class_end(p, end, &class_end)) {
                return 0;
            }
            p = class_end - 1;
            continue;
        }
        if (*p == '\\' && p + 1 < end) {
            p++;
            continue;
        }
        if (*p == '(') {
            const char *group_end = 0;
            if (!find_group_end(p, end, &group_end)) {
                return 0;
            }
            p = group_end - 1;
            continue;
        }
        if (*p == '|') {
            return p;
        }
    }
    return 0;
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
    if (p < end && (*p == '^' || *p == '!')) {
        invert = 1;
        p++;
    }
    if (p < end && *p == ']') {
        matched = same_char(c, ']', flags);
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

static int parse_number(const char **cursor, const char *end, int *out) {
    int value = 0;
    int saw_digit = 0;
    const char *p = *cursor;
    while (p < end && *p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        saw_digit = 1;
        p++;
    }
    if (!saw_digit) {
        return 0;
    }
    *cursor = p;
    *out = value;
    return 1;
}

static int parse_bounds(const char *start, const char *end, int *min_out, int *max_out, const char **after_out) {
    const char *p = start + 1;
    int min = 0;
    int max = 0;
    if (!parse_number(&p, end, &min)) {
        return 0;
    }
    max = min;
    if (p < end && *p == ',') {
        p++;
        if (p < end && *p == '}') {
            max = REGEX_MAX_REPEAT;
        } else if (!parse_number(&p, end, &max)) {
            return 0;
        }
    }
    if (p >= end || *p != '}' || max < min) {
        return 0;
    }
    if (max > REGEX_MAX_REPEAT) {
        max = REGEX_MAX_REPEAT;
    }
    *min_out = min;
    *max_out = max;
    *after_out = p + 1;
    return 1;
}

static int match_expr(const char *pattern, const char *end, const char *text,
    const struct regex_context *ctx, regmatch_t *match, const char **end_out);

static int atom_end(const char *pattern, const char *end, const char **atom_out) {
    if (pattern >= end) {
        return 0;
    }
    if (*pattern == '[') {
        return find_class_end(pattern, end, atom_out);
    }
    if (*pattern == '(') {
        return find_group_end(pattern, end, atom_out);
    }
    if (*pattern == '\\' && pattern + 1 < end) {
        *atom_out = pattern + 2;
        return 1;
    }
    *atom_out = pattern + 1;
    return 1;
}

static int match_atom_once(const char *pattern, const char *atom_end_value,
    const char *text, const struct regex_context *ctx, regmatch_t *match,
    const char **end_out) {
    if (*text == '\0') {
        return 0;
    }
    if (*pattern == '.') {
        if ((ctx->cflags & REG_NEWLINE) != 0 && *text == '\n') {
            return 0;
        }
        *end_out = text + 1;
        return 1;
    }
    if (*pattern == '[') {
        if (class_contains(pattern, atom_end_value, *text, ctx->cflags)) {
            *end_out = text + 1;
            return 1;
        }
        return 0;
    }
    if (*pattern == '(') {
        const char *group_end = atom_end_value - 1;
        const char *inner_end = 0;
        regmatch_t saved[REGEX_MAX_MATCHES];
        memcpy(saved, match, sizeof(saved));
        if (!match_expr(pattern + 1, group_end, text, ctx, match, &inner_end)) {
            memcpy(match, saved, sizeof(saved));
            return 0;
        }
        int index = capture_index_for(ctx->pattern_base, pattern);
        if (index > 0 && (size_t)index < ctx->nmatch && index < REGEX_MAX_MATCHES) {
            match[index].rm_so = (regoff_t)(text - ctx->string_base);
            match[index].rm_eo = (regoff_t)(inner_end - ctx->string_base);
        }
        *end_out = inner_end;
        return 1;
    }
    if (*pattern == '\\' && pattern + 1 < atom_end_value) {
        if (same_char(*text, pattern[1], ctx->cflags)) {
            *end_out = text + 1;
            return 1;
        }
        return 0;
    }
    if (same_char(*text, *pattern, ctx->cflags)) {
        *end_out = text + 1;
        return 1;
    }
    return 0;
}

static int match_sequence(const char *pattern, const char *end, const char *text,
    const struct regex_context *ctx, regmatch_t *match, const char **end_out) {
    if (pattern >= end) {
        *end_out = text;
        return 1;
    }
    if (*pattern == '$' && pattern + 1 == end) {
        if (*text == '\0' || ((ctx->cflags & REG_NEWLINE) != 0 && *text == '\n' && (ctx->eflags & REG_NOTEOL) == 0)) {
            *end_out = text;
            return 1;
        }
        return 0;
    }

    const char *atom = 0;
    if (!atom_end(pattern, end, &atom)) {
        return 0;
    }
    int min = 1;
    int max = 1;
    const char *rest = atom;
    if (rest < end && *rest == '*') {
        min = 0;
        max = REGEX_MAX_REPEAT;
        rest++;
    } else if (rest < end && *rest == '+' && (ctx->cflags & REG_EXTENDED) != 0) {
        min = 1;
        max = REGEX_MAX_REPEAT;
        rest++;
    } else if (rest < end && *rest == '?' && (ctx->cflags & REG_EXTENDED) != 0) {
        min = 0;
        max = 1;
        rest++;
    } else if (rest < end && *rest == '{' && (ctx->cflags & REG_EXTENDED) != 0) {
        if (!parse_bounds(rest, end, &min, &max, &rest)) {
            return 0;
        }
    }

    struct regex_state *states = calloc((size_t)max + 1, sizeof(*states));
    size_t count = 0;
    if (states == 0) {
        return 0;
    }
    states[0].text = text;
    memcpy(states[0].match, match, sizeof(states[0].match));
    while ((int)count < max) {
        const char *next_text = 0;
        regmatch_t next_match[REGEX_MAX_MATCHES];
        memcpy(next_match, states[count].match, sizeof(next_match));
        if (!match_atom_once(pattern, atom, states[count].text, ctx, next_match, &next_text) ||
            next_text == states[count].text) {
            break;
        }
        count++;
        states[count].text = next_text;
        memcpy(states[count].match, next_match, sizeof(states[count].match));
    }
    for (size_t used = count + 1; used > 0; used--) {
        size_t index = used - 1;
        if ((int)index < min) {
            break;
        }
        regmatch_t candidate[REGEX_MAX_MATCHES];
        const char *candidate_end = 0;
        memcpy(candidate, states[index].match, sizeof(candidate));
        if (match_sequence(rest, end, states[index].text, ctx, candidate, &candidate_end)) {
            memcpy(match, candidate, sizeof(candidate));
            *end_out = candidate_end;
            free(states);
            return 1;
        }
    }
    free(states);
    return 0;
}

static int match_expr(const char *pattern, const char *end, const char *text,
    const struct regex_context *ctx, regmatch_t *match, const char **end_out) {
    const char *alt = find_top_level_alt(pattern, end);
    if (alt != 0) {
        regmatch_t saved[REGEX_MAX_MATCHES];
        memcpy(saved, match, sizeof(saved));
        if (match_sequence(pattern, alt, text, ctx, match, end_out)) {
            return 1;
        }
        memcpy(match, saved, sizeof(saved));
        return match_expr(alt + 1, end, text, ctx, match, end_out);
    }
    return match_sequence(pattern, end, text, ctx, match, end_out);
}

static int validate_pattern(const char *pattern, int extended) {
    const char *p = pattern;
    int previous_atom = 0;
    while (*p != '\0') {
        const char *end = 0;
        if (*p == '[') {
            if (!find_class_end(p, p + strlen(p), &end)) {
                return REG_EBRACK;
            }
            p = end;
            previous_atom = 1;
        } else if (*p == '(') {
            if (!extended) {
                return REG_BADPAT;
            }
            if (!find_group_end(p, p + strlen(p), &end)) {
                return REG_EPAREN;
            }
            p = end;
            previous_atom = 1;
        } else if (*p == ')') {
            return REG_EPAREN;
        } else if (*p == '|') {
            if (!extended) {
                return REG_BADPAT;
            }
            previous_atom = 0;
            p++;
        } else if (*p == '\\') {
            if (p[1] == '\0') {
                return REG_EESCAPE;
            }
            p += 2;
            previous_atom = 1;
        } else if (*p == '*' || (extended && (*p == '+' || *p == '?'))) {
            if (!previous_atom) {
                return REG_BADRPT;
            }
            p++;
            previous_atom = 0;
        } else if (*p == '{' && extended) {
            int min = 0;
            int max = 0;
            if (!previous_atom || !parse_bounds(p, p + strlen(p), &min, &max, &end)) {
                return REG_BADBR;
            }
            (void)min;
            (void)max;
            p = end;
            previous_atom = 0;
        } else {
            p++;
            previous_atom = 1;
        }
    }
    return REG_OK;
}

static size_t count_captures(const char *pattern) {
    size_t count = 0;
    const char *limit = pattern + strlen(pattern);
    for (const char *p = pattern; *p != '\0'; p++) {
        if (*p == '[') {
            const char *end = 0;
            if (find_class_end(p, limit, &end)) {
                p = end - 1;
            }
        } else if (*p == '\\' && p[1] != '\0') {
            p++;
        } else if (*p == '(') {
            count++;
        }
    }
    return count;
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
    preg->re_nsub = count_captures(regex);
    return REG_OK;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch,
    regmatch_t pmatch[], int eflags) {
    const char *pattern;
    const char *pattern_end;
    const char *text;
    const char *end = 0;
    regmatch_t match[REGEX_MAX_MATCHES];
    size_t internal_nmatch = nmatch < REGEX_MAX_MATCHES ? nmatch : REGEX_MAX_MATCHES;
    if (preg == 0 || preg->pattern == 0 || string == 0) {
        return REG_NOMATCH;
    }
    pattern = preg->pattern;
    pattern_end = pattern + strlen(pattern);
    struct regex_context ctx = {
        .pattern_base = pattern,
        .string_base = string,
        .cflags = preg->cflags,
        .eflags = eflags,
        .nmatch = internal_nmatch,
    };
    int anchored = pattern < pattern_end && *pattern == '^';
    if (anchored) {
        pattern++;
    }
    for (text = string;; text++) {
        init_matches(match, REGEX_MAX_MATCHES);
        if (match_expr(pattern, pattern_end, text, &ctx, match, &end)) {
            if (nmatch > 0 && pmatch != 0) {
                pmatch[0].rm_so = (regoff_t)(text - string);
                pmatch[0].rm_eo = (regoff_t)(end - string);
                for (size_t i = 1; i < nmatch; i++) {
                    if (i < REGEX_MAX_MATCHES) {
                        pmatch[i] = match[i];
                    } else {
                        pmatch[i].rm_so = -1;
                        pmatch[i].rm_eo = -1;
                    }
                }
            }
            return REG_OK;
        }
        if (anchored || *text == '\0') {
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
    case REG_EPAREN: message = "unmatched parenthesis"; break;
    case REG_EBRACE:
    case REG_BADBR: message = "bad bounds"; break;
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
