#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "native_json.h"

static int failures;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        failures++; \
    } \
} while (0)

static int parse_cstr(NJDoc *doc, const char *json) {
    return nj_parse(doc, json, strlen(json));
}

static void test_nested_structure_and_navigation(void) {
    static const char json[] =
        "{\"outer\":{\"items\":[1,{\"target\":42}],\"empty\":[]},"
        "\"target\":7,\"blank\":\"\",\"nothing\":null,\"flag\":true}";
    NJDoc doc = {0};
    int outer, target, items, nested;

    CHECK(parse_cstr(&doc, json));
    CHECK(doc.count == 19);
    CHECK(doc.tokens[0].kind == NJ_OBJECT);
    CHECK(doc.tokens[0].count == 5);
    CHECK(doc.tokens[0].next == doc.count);

    outer = nj_get(&doc, 0, "outer");
    target = nj_get(&doc, 0, "target");
    CHECK(outer >= 0 && doc.tokens[outer].kind == NJ_OBJECT);
    CHECK(target >= 0 && nj_int(&doc, target, -1) == 7);
    CHECK(nj_get(&doc, 0, "missing") == -1);
    CHECK(nj_get(&doc, -1, "target") == -1);

    items = nj_get(&doc, outer, "items");
    CHECK(items >= 0 && doc.tokens[items].kind == NJ_ARRAY);
    CHECK(doc.tokens[items].count == 2);
    nested = items + 2;
    CHECK(doc.tokens[nested].kind == NJ_OBJECT);
    CHECK(nj_int(&doc, nj_get(&doc, nested, "target"), -1) == 42);
    CHECK(doc.tokens[outer].next == target - 1);

    nj_free(&doc);
    CHECK(doc.text == NULL && doc.tokens == NULL && doc.count == 0 && doc.length == 0);
}

static void test_empty_values(void) {
    NJDoc doc = {0};
    wchar_t out[2] = {L'x', L'x'};
    int s;

    CHECK(parse_cstr(&doc, "{\"o\":{},\"a\":[],\"s\":\"\"}"));
    CHECK(doc.tokens[nj_get(&doc, 0, "o")].count == 0);
    CHECK(doc.tokens[nj_get(&doc, 0, "a")].count == 0);
    s = nj_get(&doc, 0, "s");
    CHECK(nj_string(&doc, s, out, 1));
    CHECK(out[0] == L'\0');
    nj_free(&doc);
}

static void test_unicode_and_escapes(void) {
    static const char json[] =
        "{\"raw\":\"\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82\","
        "\"escaped\":\"line\\nquote\\\"slash\\\\tab\\t\","
        "\"emoji\":\"\\uD83D\\uDE03\"}";
    NJDoc doc = {0};
    wchar_t out[64];
    wchar_t tiny[2] = {L'x', L'y'};

    CHECK(parse_cstr(&doc, json));
    CHECK(nj_string(&doc, nj_get(&doc, 0, "raw"), out, 64));
    CHECK(wcscmp(out, L"\x041F\x0440\x0438\x0432\x0435\x0442") == 0);
    CHECK(nj_string(&doc, nj_get(&doc, 0, "escaped"), out, 64));
    CHECK(wcscmp(out, L"line\nquote\"slash\\tab\t") == 0);
    CHECK(nj_string(&doc, nj_get(&doc, 0, "emoji"), out, 64));
    CHECK(out[0] == (wchar_t)0xD83D && out[1] == (wchar_t)0xDE03 && out[2] == 0);
    CHECK(!nj_string(&doc, nj_get(&doc, 0, "emoji"), tiny, 2));
    CHECK(tiny[0] == L'x' && tiny[1] == L'y');
    CHECK(!nj_string(&doc, 0, out, 64));
    CHECK(!nj_string(&doc, -1, out, 64));
    nj_free(&doc);
}

static void test_escaped_nul_key_does_not_match_prefix(void) {
    static const char json[] = "{\"issues\\u0000\":1,\"issues\\u0000x\":2}";
    char lookup[8] = {'i', 's', 's', 'u', 'e', 's', '\0', '\0'};
    NJDoc doc = {0};

    CHECK(parse_cstr(&doc, json));
    CHECK(nj_get(&doc, 0, lookup) == -1);
    nj_free(&doc);
}

static void test_numbers_and_integer_bounds(void) {
    NJDoc doc = {0};
    int fallback = 12345;

    CHECK(parse_cstr(&doc,
        "{\"min\":-2147483648,\"max\":2147483647,\"over\":2147483648,"
        "\"under\":-2147483649,\"frac\":1.5,\"exp\":1e2}"));
    CHECK(nj_int(&doc, nj_get(&doc, 0, "min"), fallback) == INT_MIN);
    CHECK(nj_int(&doc, nj_get(&doc, 0, "max"), fallback) == INT_MAX);
    CHECK(nj_int(&doc, nj_get(&doc, 0, "over"), fallback) == fallback);
    CHECK(nj_int(&doc, nj_get(&doc, 0, "under"), fallback) == fallback);
    CHECK(nj_int(&doc, nj_get(&doc, 0, "frac"), fallback) == fallback);
    CHECK(nj_int(&doc, nj_get(&doc, 0, "exp"), fallback) == fallback);
    CHECK(nj_int(&doc, -1, fallback) == fallback);
    nj_free(&doc);
}

static void expect_invalid(const char *bytes, size_t length) {
    NJDoc doc = {(const char *)1, 9, (NJToken *)1, 7};
    CHECK(!nj_parse(&doc, bytes, length));
    CHECK(doc.text == NULL && doc.length == 0 && doc.tokens == NULL && doc.count == 0);
}

static void test_invalid_payloads(void) {
    static const char raw_control[] = {'"', 'a', 1, 'b', '"'};
    static const char bad_utf8_1[] = {'"', (char)0xC0, (char)0xAF, '"'};
    static const char bad_utf8_2[] = {'"', (char)0xE2, (char)0x82, '"'};
    static const char embedded_nul[] = {'n','u','l','l','\0','x'};
    static const char *bad[] = {
        "", " ", "{}x", "[1,]", "[,1]", "{\"a\":1,}", "{\"a\" 1}",
        "{1:2}", "[1 2]", "01", "-", "1.", "1e", "NaN", "TRUE",
        "\"\\x00\"", "\"\\u12G4\"", "\"\\uD800\"", "\"\\uDC00\"",
        "\"\\uD800\\u0041\"", "\"unterminated"
    };
    size_t i;

    for (i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        expect_invalid(bad[i], strlen(bad[i]));
    expect_invalid(raw_control, sizeof raw_control);
    expect_invalid(bad_utf8_1, sizeof bad_utf8_1);
    expect_invalid(bad_utf8_2, sizeof bad_utf8_2);
    expect_invalid(embedded_nul, sizeof embedded_nul);
    {
        NJDoc bounded = {0};
        CHECK(nj_parse(&bounded, "trueX", 4));
        CHECK(bounded.count == 1 && bounded.tokens[0].kind == NJ_BOOL);
        nj_free(&bounded);
    }
    expect_invalid(NULL, 1);
    CHECK(!nj_parse(NULL, "null", 4));
}

static char *nested_arrays(int depth) {
    size_t n = (size_t)depth * 2 + 2;
    char *s = (char *)malloc(n);
    int i;
    if (!s) return NULL;
    for (i = 0; i < depth; ++i) s[i] = '[';
    s[depth] = '0';
    for (i = 0; i < depth; ++i) s[depth + 1 + i] = ']';
    s[depth * 2 + 1] = 0;
    return s;
}

static void test_depth_and_length_limits(void) {
    NJDoc doc = {0};
    char *at_limit = nested_arrays(64);
    char *too_deep = nested_arrays(65);
    char *too_long = (char *)malloc(16u * 1024u * 1024u + 1u);

    CHECK(at_limit != NULL && parse_cstr(&doc, at_limit));
    nj_free(&doc);
    CHECK(too_deep != NULL);
    if (too_deep) expect_invalid(too_deep, strlen(too_deep));
    CHECK(too_long != NULL);
    if (too_long) {
        too_long[0] = '0';
        expect_invalid(too_long, 16u * 1024u * 1024u + 1u);
    }
    free(at_limit);
    free(too_deep);
    free(too_long);
}

int main(void) {
    test_nested_structure_and_navigation();
    test_empty_values();
    test_unicode_and_escapes();
    test_escaped_nul_key_does_not_match_prefix();
    test_numbers_and_integer_bounds();
    test_invalid_payloads();
    test_depth_and_length_limits();
    if (failures) {
        fprintf(stderr, "%d test failure(s)\n", failures);
        return 1;
    }
    puts("native_json_test: all tests passed");
    return 0;
}
