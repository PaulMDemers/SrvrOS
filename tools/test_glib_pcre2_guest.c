/* Exercise the target PCRE2 8-bit interpreter used by GLib's regex layer. */
#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { \
    if (!(x)) { \
        printf("glib-pcre2: failure line=%d\n", __LINE__); \
        return 1; \
    } \
} while (0)

int main(void) {
    uint32_t unicode = 0, jit = 1, widths = 0;
    PCRE2_UCHAR version[64] = {0};
    CHECK(pcre2_config(PCRE2_CONFIG_UNICODE, &unicode) == 0 && unicode != 0);
    CHECK(pcre2_config(PCRE2_CONFIG_JIT, &jit) == 0 && jit == 0);
    CHECK(pcre2_config(PCRE2_CONFIG_COMPILED_WIDTHS, &widths) == 0 && widths == 1);
    CHECK(pcre2_config(PCRE2_CONFIG_VERSION, version) > 0);
    CHECK(strncmp((const char *)version, "10.49", 5) == 0);

    int error = 0;
    PCRE2_SIZE error_offset = 0;
    pcre2_code *code = pcre2_compile((PCRE2_SPTR)"^(?<word>\\p{L}+)-(\\d+)$",
        PCRE2_ZERO_TERMINATED, PCRE2_UTF | PCRE2_UCP, &error, &error_offset, NULL);
    CHECK(code != NULL);
    pcre2_match_data *match = pcre2_match_data_create_from_pattern(code, NULL);
    CHECK(match != NULL);
    const unsigned char subject[] = {'c', 'a', 'f', 0xc3, 0xa9, '-', '4', '2'};
    CHECK(pcre2_match(code, subject, sizeof(subject), 0, 0, match, NULL) == 3);
    PCRE2_SIZE *offsets = pcre2_get_ovector_pointer(match);
    CHECK(offsets[0] == 0 && offsets[1] == 8);
    CHECK(offsets[2] == 0 && offsets[3] == 5);
    CHECK(offsets[4] == 6 && offsets[5] == 8);
    CHECK(pcre2_substring_number_from_name(code, (PCRE2_SPTR)"word") == 1);
    CHECK(pcre2_match(code, (PCRE2_SPTR)"no-match", 8, 0, 0, match, NULL) == PCRE2_ERROR_NOMATCH);
    const unsigned char invalid[] = {0xc3, '('};
    int invalid_result = pcre2_match(code, invalid, sizeof(invalid), 0, 0, match, NULL);
    CHECK(invalid_result <= PCRE2_ERROR_UTF8_ERR1 && invalid_result >= PCRE2_ERROR_UTF8_ERR21);
    pcre2_match_data_free(match);
    pcre2_code_free(code);

    code = pcre2_compile((PCRE2_SPTR)"(", PCRE2_ZERO_TERMINATED, 0,
        &error, &error_offset, NULL);
    CHECK(code == NULL && error != 0 && error_offset <= 1);

    code = pcre2_compile((PCRE2_SPTR)"(*NO_AUTO_POSSESS)(*NO_START_OPT)^(a+)+$",
        PCRE2_ZERO_TERMINATED, 0, &error, &error_offset, NULL);
    CHECK(code != NULL);
    match = pcre2_match_data_create_from_pattern(code, NULL);
    pcre2_match_context *context = pcre2_match_context_create(NULL);
    CHECK(match != NULL && context != NULL);
    CHECK(pcre2_set_match_limit(context, 1) == 0);
    CHECK(pcre2_match(code, (PCRE2_SPTR)"aaaaaaaaaaaa!", 13, 0, 0,
        match, context) == PCRE2_ERROR_MATCHLIMIT);
    pcre2_match_context_free(context);
    pcre2_match_data_free(match);
    pcre2_code_free(code);
    puts("glib-pcre2: ok");
    return 0;
}
