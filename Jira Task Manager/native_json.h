#ifndef FTASKS_NATIVE_JSON_H
#define FTASKS_NATIVE_JSON_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct NJToken { int start,end,next,count; int kind; } NJToken;
typedef struct NJDoc { const char *text; size_t length; NJToken *tokens; int count; } NJDoc;
enum NJKind { NJ_OBJECT=1,NJ_ARRAY,NJ_STRING,NJ_NUMBER,NJ_BOOL,NJ_NULL };

#define NJ_MAX_INPUT (16u * 1024u * 1024u)
#define NJ_MAX_DEPTH 64
#define NJ_MAX_TOKENS (1024u * 1024u)

typedef struct NJParser {
    const char *text;
    size_t length;
    size_t pos;
    NJToken *tokens;
    size_t count;
    size_t capacity;
} NJParser;

static inline int nj__utf8(const char *s, size_t length, size_t *at, uint32_t *codepoint) {
    size_t i = *at;
    unsigned char a, b, c, d;
    uint32_t cp;
    if (i >= length) return 0;
    a = (unsigned char)s[i++];
    if (a < 0x80) {
        *codepoint = a;
        *at = i;
        return 1;
    }
    if (a < 0xC2 || a > 0xF4 || i >= length) return 0;
    b = (unsigned char)s[i++];
    if ((b & 0xC0) != 0x80) return 0;
    if (a < 0xE0) {
        cp = ((uint32_t)(a & 0x1F) << 6) | (uint32_t)(b & 0x3F);
    } else {
        if (i >= length) return 0;
        c = (unsigned char)s[i++];
        if ((c & 0xC0) != 0x80) return 0;
        if (a == 0xE0 && b < 0xA0) return 0;
        if (a == 0xED && b >= 0xA0) return 0;
        if (a < 0xF0) {
            cp = ((uint32_t)(a & 0x0F) << 12) |
                 ((uint32_t)(b & 0x3F) << 6) | (uint32_t)(c & 0x3F);
        } else {
            if (i >= length) return 0;
            d = (unsigned char)s[i++];
            if ((d & 0xC0) != 0x80) return 0;
            if (a == 0xF0 && b < 0x90) return 0;
            if (a == 0xF4 && b >= 0x90) return 0;
            cp = ((uint32_t)(a & 7) << 18) | ((uint32_t)(b & 0x3F) << 12) |
                 ((uint32_t)(c & 0x3F) << 6) | (uint32_t)(d & 0x3F);
        }
    }
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
    *codepoint = cp;
    *at = i;
    return 1;
}

static inline int nj__hex4(const char *s, size_t length, size_t at, uint32_t *value) {
    uint32_t v = 0;
    size_t i;
    if (length - at < 4) return 0;
    for (i = 0; i < 4; ++i) {
        unsigned char ch = (unsigned char)s[at + i];
        unsigned digit;
        if (ch >= '0' && ch <= '9') digit = ch - '0';
        else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10u;
        else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10u;
        else return 0;
        v = (v << 4) | digit;
    }
    *value = v;
    return 1;
}

static inline int nj__reserve(NJParser *p) {
    NJToken *grown;
    size_t capacity;
    if (p->count < p->capacity) return 1;
    if (p->count >= NJ_MAX_TOKENS) return 0;
    capacity = p->capacity ? p->capacity * 2u : 64u;
    if (capacity > NJ_MAX_TOKENS) capacity = NJ_MAX_TOKENS;
    if (capacity > SIZE_MAX / sizeof *p->tokens) return 0;
    grown = (NJToken *)realloc(p->tokens, capacity * sizeof *p->tokens);
    if (!grown) return 0;
    p->tokens = grown;
    p->capacity = capacity;
    return 1;
}

static inline int nj__add(NJParser *p, int kind, size_t start) {
    NJToken *token;
    if (start > (size_t)INT_MAX || !nj__reserve(p)) return -1;
    token = &p->tokens[p->count];
    token->start = (int)start;
    token->end = (int)start;
    token->next = (int)p->count + 1;
    token->count = 0;
    token->kind = kind;
    return (int)p->count++;
}

static inline void nj__space(NJParser *p) {
    while (p->pos < p->length) {
        char ch = p->text[p->pos];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') break;
        ++p->pos;
    }
}

static inline int nj__string_token(NJParser *p) {
    int token;
    size_t content;
    if (p->pos >= p->length || p->text[p->pos] != '"') return -1;
    content = ++p->pos;
    token = nj__add(p, NJ_STRING, content);
    if (token < 0) return -1;
    while (p->pos < p->length) {
        unsigned char ch = (unsigned char)p->text[p->pos];
        if (ch == '"') {
            p->tokens[token].end = (int)p->pos++;
            p->tokens[token].next = (int)p->count;
            return token;
        }
        if (ch < 0x20) return -1;
        if (ch == '\\') {
            uint32_t u, low;
            ++p->pos;
            if (p->pos >= p->length) return -1;
            ch = (unsigned char)p->text[p->pos++];
            if (ch == '"' || ch == '\\' || ch == '/' || ch == 'b' ||
                ch == 'f' || ch == 'n' || ch == 'r' || ch == 't') continue;
            if (ch != 'u' || !nj__hex4(p->text, p->length, p->pos, &u)) return -1;
            p->pos += 4;
            if (u >= 0xD800 && u <= 0xDBFF) {
                if (p->length - p->pos < 6 || p->text[p->pos] != '\\' ||
                    p->text[p->pos + 1] != 'u' ||
                    !nj__hex4(p->text, p->length, p->pos + 2, &low) ||
                    low < 0xDC00 || low > 0xDFFF) return -1;
                p->pos += 6;
            } else if (u >= 0xDC00 && u <= 0xDFFF) {
                return -1;
            }
        } else if (ch < 0x80) {
            ++p->pos;
        } else {
            uint32_t cp;
            size_t at = p->pos;
            if (!nj__utf8(p->text, p->length, &at, &cp)) return -1;
            p->pos = at;
        }
    }
    return -1;
}

static inline int nj__value(NJParser *p, int depth);

static inline int nj__container(NJParser *p, int depth, int is_object) {
    int token, child;
    char close = is_object ? '}' : ']';
    if (depth > NJ_MAX_DEPTH) return -1;
    token = nj__add(p, is_object ? NJ_OBJECT : NJ_ARRAY, p->pos);
    if (token < 0) return -1;
    ++p->pos;
    nj__space(p);
    if (p->pos < p->length && p->text[p->pos] == close) {
        p->tokens[token].end = (int)++p->pos;
        p->tokens[token].next = (int)p->count;
        return token;
    }
    for (;;) {
        if (is_object) {
            child = nj__string_token(p);
            if (child < 0) return -1;
            nj__space(p);
            if (p->pos >= p->length || p->text[p->pos++] != ':') return -1;
            nj__space(p);
        }
        child = nj__value(p, depth + 1);
        if (child < 0) return -1;
        ++p->tokens[token].count;
        nj__space(p);
        if (p->pos >= p->length) return -1;
        if (p->text[p->pos] == close) {
            p->tokens[token].end = (int)++p->pos;
            p->tokens[token].next = (int)p->count;
            return token;
        }
        if (p->text[p->pos++] != ',') return -1;
        nj__space(p);
    }
}

static inline int nj__number(NJParser *p) {
    size_t start = p->pos;
    int token;
    if (p->text[p->pos] == '-') {
        if (++p->pos >= p->length) return -1;
    }
    if (p->text[p->pos] == '0') {
        ++p->pos;
        if (p->pos < p->length && p->text[p->pos] >= '0' && p->text[p->pos] <= '9') return -1;
    } else {
        if (p->text[p->pos] < '1' || p->text[p->pos] > '9') return -1;
        do { ++p->pos; }
        while (p->pos < p->length && p->text[p->pos] >= '0' && p->text[p->pos] <= '9');
    }
    if (p->pos < p->length && p->text[p->pos] == '.') {
        ++p->pos;
        if (p->pos >= p->length || p->text[p->pos] < '0' || p->text[p->pos] > '9') return -1;
        do { ++p->pos; }
        while (p->pos < p->length && p->text[p->pos] >= '0' && p->text[p->pos] <= '9');
    }
    if (p->pos < p->length && (p->text[p->pos] == 'e' || p->text[p->pos] == 'E')) {
        ++p->pos;
        if (p->pos < p->length && (p->text[p->pos] == '+' || p->text[p->pos] == '-')) ++p->pos;
        if (p->pos >= p->length || p->text[p->pos] < '0' || p->text[p->pos] > '9') return -1;
        do { ++p->pos; }
        while (p->pos < p->length && p->text[p->pos] >= '0' && p->text[p->pos] <= '9');
    }
    token = nj__add(p, NJ_NUMBER, start);
    if (token >= 0) {
        p->tokens[token].end = (int)p->pos;
        p->tokens[token].next = (int)p->count;
    }
    return token;
}

static inline int nj__literal(NJParser *p, const char *word, size_t n, int kind) {
    size_t start = p->pos;
    int token;
    if (p->length - p->pos < n || memcmp(p->text + p->pos, word, n) != 0) return -1;
    p->pos += n;
    token = nj__add(p, kind, start);
    if (token >= 0) {
        p->tokens[token].end = (int)p->pos;
        p->tokens[token].next = (int)p->count;
    }
    return token;
}

static inline int nj__value(NJParser *p, int depth) {
    if (p->pos >= p->length) return -1;
    switch (p->text[p->pos]) {
        case '{': return nj__container(p, depth, 1);
        case '[': return nj__container(p, depth, 0);
        case '"': return nj__string_token(p);
        case 't': return nj__literal(p, "true", 4, NJ_BOOL);
        case 'f': return nj__literal(p, "false", 5, NJ_BOOL);
        case 'n': return nj__literal(p, "null", 4, NJ_NULL);
        default: return nj__number(p);
    }
}

static inline void nj_free(NJDoc *doc) {
    if (!doc) return;
    free(doc->tokens);
    doc->text = NULL;
    doc->length = 0;
    doc->tokens = NULL;
    doc->count = 0;
}

static inline int nj_parse(NJDoc *doc, const char *text, size_t length) {
    NJParser p;
    int root;
    if (!doc) return 0;
    doc->text = NULL;
    doc->length = 0;
    doc->tokens = NULL;
    doc->count = 0;
    if ((!text && length != 0) || length == 0 || length > NJ_MAX_INPUT) return 0;
    memset(&p, 0, sizeof p);
    p.text = text;
    p.length = length;
    nj__space(&p);
    root = nj__value(&p, 1);
    nj__space(&p);
    if (root != 0 || p.pos != p.length || p.count > (size_t)INT_MAX) {
        free(p.tokens);
        return 0;
    }
    doc->text = text;
    doc->length = length;
    doc->tokens = p.tokens;
    doc->count = (int)p.count;
    return 1;
}

static inline int nj__key_equal(const NJDoc *doc, const NJToken *token, const char *key) {
    size_t pos = (size_t)token->start;
    size_t end = (size_t)token->end;
    size_t k = 0;
    while (pos < end) {
        uint32_t cp;
        unsigned char ch = (unsigned char)doc->text[pos];
        if (ch == '\\') {
            ch = (unsigned char)doc->text[++pos];
            ++pos;
            if (ch == 'u') {
                if (!nj__hex4(doc->text, end, pos, &cp)) return 0;
                pos += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF) return 0;
            } else {
                switch (ch) {
                    case 'b': cp = '\b'; break; case 'f': cp = '\f'; break;
                    case 'n': cp = '\n'; break; case 'r': cp = '\r'; break;
                    case 't': cp = '\t'; break; default: cp = ch; break;
                }
            }
        } else if (ch < 0x80) {
            cp = ch;
            ++pos;
        } else if (!nj__utf8(doc->text, end, &pos, &cp)) {
            return 0;
        }
        if (cp == 0 || cp > 0x7F || key[k] == '\0' ||
            (unsigned char)key[k] != (unsigned char)cp) return 0;
        ++k;
    }
    return key[k] == '\0';
}

static inline int nj_get(const NJDoc *doc, int object, const char *ascii_key) {
    int at, pair;
    const NJToken *container;
    if (!doc || !doc->tokens || !ascii_key || object < 0 || object >= doc->count) return -1;
    container = &doc->tokens[object];
    if (container->kind != NJ_OBJECT) return -1;
    at = object + 1;
    for (pair = 0; pair < container->count; ++pair) {
        int value;
        if (at < 0 || at >= doc->count || doc->tokens[at].kind != NJ_STRING) return -1;
        value = at + 1;
        if (value >= doc->count) return -1;
        if (nj__key_equal(doc, &doc->tokens[at], ascii_key)) return value;
        at = doc->tokens[value].next;
    }
    return -1;
}

static inline int nj_int(const NJDoc *doc, int token, int fallback) {
    const NJToken *t;
    size_t pos, end;
    unsigned long long value = 0;
    unsigned long long limit;
    int negative = 0;
    if (!doc || !doc->tokens || token < 0 || token >= doc->count) return fallback;
    t = &doc->tokens[token];
    if (t->kind != NJ_NUMBER || t->start < 0 || t->end <= t->start) return fallback;
    pos = (size_t)t->start;
    end = (size_t)t->end;
    if (doc->text[pos] == '-') { negative = 1; ++pos; }
    limit = negative ? (unsigned long long)INT_MAX + 1u : (unsigned long long)INT_MAX;
    if (pos >= end) return fallback;
    while (pos < end) {
        unsigned digit;
        char ch = doc->text[pos++];
        if (ch < '0' || ch > '9') return fallback;
        digit = (unsigned)(ch - '0');
        if (value > (limit - digit) / 10u) return fallback;
        value = value * 10u + digit;
    }
    if (negative) {
        if (value == (unsigned long long)INT_MAX + 1u) return INT_MIN;
        return -(int)value;
    }
    return (int)value;
}

static inline int nj__string_units(const NJDoc *doc, const NJToken *t, size_t *units) {
    size_t pos = (size_t)t->start, end = (size_t)t->end, count = 0;
    while (pos < end) {
        uint32_t cp;
        unsigned char ch = (unsigned char)doc->text[pos];
        if (ch == '\\') {
            ch = (unsigned char)doc->text[++pos];
            ++pos;
            if (ch == 'u') {
                if (!nj__hex4(doc->text, end, pos, &cp)) return 0;
                pos += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    uint32_t low;
                    if (end - pos < 6 || !nj__hex4(doc->text, end, pos + 2, &low)) return 0;
                    pos += 6;
                    count += 2;
                    continue;
                }
            } else cp = ch;
        } else if (ch < 0x80) {
            cp = ch;
            ++pos;
        } else if (!nj__utf8(doc->text, end, &pos, &cp)) return 0;
        count += cp > 0xFFFF ? 2u : 1u;
    }
    *units = count;
    return 1;
}

static inline int nj_string(const NJDoc *doc, int token, wchar_t *out, size_t capacity) {
    const NJToken *t;
    size_t pos, end, units, written = 0;
    if (!doc || !doc->tokens || !out || token < 0 || token >= doc->count) return 0;
    t = &doc->tokens[token];
    if (t->kind != NJ_STRING || !nj__string_units(doc, t, &units) || capacity <= units) return 0;
    pos = (size_t)t->start;
    end = (size_t)t->end;
    while (pos < end) {
        uint32_t cp;
        unsigned char ch = (unsigned char)doc->text[pos];
        if (ch == '\\') {
            ch = (unsigned char)doc->text[++pos];
            ++pos;
            if (ch == 'u') {
                uint32_t high, low;
                (void)nj__hex4(doc->text, end, pos, &high);
                pos += 4;
                if (high >= 0xD800 && high <= 0xDBFF) {
                    (void)nj__hex4(doc->text, end, pos + 2, &low);
                    pos += 6;
                    out[written++] = (wchar_t)high;
                    out[written++] = (wchar_t)low;
                    continue;
                }
                cp = high;
            } else {
                switch (ch) {
                    case 'b': cp = '\b'; break; case 'f': cp = '\f'; break;
                    case 'n': cp = '\n'; break; case 'r': cp = '\r'; break;
                    case 't': cp = '\t'; break; default: cp = ch; break;
                }
            }
        } else if (ch < 0x80) {
            cp = ch;
            ++pos;
        } else {
            (void)nj__utf8(doc->text, end, &pos, &cp);
        }
        if (cp <= 0xFFFF) out[written++] = (wchar_t)cp;
        else {
            cp -= 0x10000;
            out[written++] = (wchar_t)(0xD800u + (cp >> 10));
            out[written++] = (wchar_t)(0xDC00u + (cp & 0x3FF));
        }
    }
    out[written] = L'\0';
    return 1;
}

#endif
