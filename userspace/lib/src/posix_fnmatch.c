#include <fnmatch.h>

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <wctype.h>

/* SrvrOS has one C locale. Each nonzero input byte is a character, and
 * character classification and case folding use the SDK's ASCII tables. */
enum atom_kind { ATOM_INVALID, ATOM_BYTE, ATOM_EQUIVALENT, ATOM_CLASS };

struct bracket_atom {
    const unsigned char *next;
    enum atom_kind kind;
    unsigned char byte;
    wctype_t char_class;
};

static struct bracket_atom read_atom(const unsigned char *p, int flags) {
    if (*p == '\\' && !(flags & FNM_NOESCAPE)) {
        if (!p[1]) return (struct bracket_atom){0};
        return (struct bracket_atom){.next = p + 2, .kind = ATOM_BYTE, .byte = p[1]};
    }
    if (*p != '[' || (p[1] != ':' && p[1] != '.' && p[1] != '=')) {
        return (struct bracket_atom){.next = p + 1, .kind = ATOM_BYTE, .byte = *p};
    }

    unsigned char marker = p[1];
    const unsigned char *name = p + 2;
    const unsigned char *end = name;
    while (*end && (*end != marker || end[1] != ']')) ++end;
    if (!*end) return (struct bracket_atom){0};

    struct bracket_atom atom = {.next = end + 2, .kind = ATOM_INVALID};
    size_t length = (size_t)(end - name);
    if (marker == ':') {
        char property[7]; /* The longest C-locale class names have six bytes. */
        if (length < sizeof(property)) {
            for (size_t i = 0; i < length; ++i) property[i] = (char)name[i];
            property[length] = '\0';
            atom.char_class = wctype(property);
            if (atom.char_class) atom.kind = ATOM_CLASS;
        }
    } else if (length == 1) {
        atom.kind = marker == '=' ? ATOM_EQUIVALENT : ATOM_BYTE;
        atom.byte = *name;
    }
    return atom;
}

static bool in_range(int value, int low, int high) {
    return value >= low && value <= high;
}

/* Return the next token even if a closed bracket expression does not match.
 * A missing closing bracket instead leaves '[' available as a literal. */
static const unsigned char *match_bracket(const unsigned char *p, int byte,
                                          int flags, bool *matched) {
    bool negated = *++p == '!';
    if (negated) ++p;
    const unsigned char *first = p;
    bool found = false;
    bool valid = true;
    int alternate = byte;
    if (flags & FNM_CASEFOLD) {
        int upper = toupper(byte);
        alternate = upper == byte ? tolower(byte) : upper;
    }

    while (*p && (p == first || *p != ']')) {
        bool leading_dash = p == first && *p == '-';
        struct bracket_atom atom = read_atom(p, flags);
        if (!atom.next) return NULL;
        p = atom.next;
        if (atom.kind == ATOM_BYTE && !leading_dash && *p == '-' && p[1] && p[1] != ']') {
            struct bracket_atom last = read_atom(p + 1, flags);
            if (last.kind == ATOM_BYTE) {
                found |= in_range(byte, atom.byte, last.byte) ||
                         in_range(alternate, atom.byte, last.byte);
                p = last.next;
                continue;
            }
        }
        if (atom.kind == ATOM_CLASS) {
            found |= iswctype(byte, atom.char_class) || iswctype(alternate, atom.char_class);
        } else if (atom.kind == ATOM_INVALID) {
            valid = false;
        } else {
            found |= atom.byte == byte || atom.byte == alternate;
        }
    }
    if (!*p) return NULL;
    *matched = valid && (found != negated);
    return p + 1;
}

static size_t match_token(const unsigned char *p, int byte, int flags, bool wildcards) {
    int literal = *p;
    size_t length = 1;
    if (literal == '\\' && !(flags & FNM_NOESCAPE)) {
        literal = p[1];
        length = 2;
    } else if (wildcards) {
        if (literal == '?') return length;
        if (literal == '[') {
            bool matched;
            const unsigned char *next = match_bracket(p, byte, flags, &matched);
            if (next) return matched ? (size_t)(next - p) : 0;
        }
    }
    bool equal = flags & FNM_CASEFOLD ? tolower(literal) == tolower(byte) : literal == byte;
    return equal ? length : 0;
}

int fnmatch(const char *pattern, const char *string, int flags) {
    const unsigned char *p = (const unsigned char *)pattern;
    const unsigned char *start = (const unsigned char *)string;
    const unsigned char *s = start;
    const unsigned char *after_star = NULL;
    const unsigned char *star_end = NULL;
    bool pathname = (flags & FNM_PATHNAME) != 0;

    /* The latest star can absorb one more byte after a mismatch. Earlier
     * stars need no revisiting because every other token matches one byte. */
    while (*s) {
        bool separator = pathname && *s == '/';
        bool leading_period = (flags & FNM_PERIOD) && *s == '.' &&
                              (s == start || (pathname && s[-1] == '/'));
        if (*p == '*' && !leading_period) {
            after_star = ++p;
            star_end = s;
            continue;
        }
        size_t length = match_token(p, *s, flags, !separator && !leading_period);
        if (length) {
            p += length;
            ++s;
            if (separator) after_star = NULL;
        } else if (after_star && *star_end && !(pathname && *star_end == '/')) {
            s = ++star_end;
            p = after_star;
        } else {
            return FNM_NOMATCH;
        }
    }
    while (*p == '*') ++p;
    return *p ? FNM_NOMATCH : 0;
}
