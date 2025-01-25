#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define MAX(x, y) (((x) >= (y)) ? (x) : (y))

typedef unsigned long long int u64;
typedef long long int s64;

void *xrealloc(void *ptr, size_t num_bytes) {
    ptr = realloc(ptr, num_bytes);
    if (!ptr) {
        perror("xrealloc failed");
        exit(1);
    }
    return ptr;
}

void *xmalloc(size_t num_bytes) {
    void *ptr = malloc(num_bytes);
    if (!ptr) {
        perror("xmalloc failed");
        exit(1);
    }
    return ptr;
}

void fatal(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    printf("FATAL: ");
    vprintf(fmt, args);
    printf("\n");

    va_end(args);
    exit(1);
}

typedef struct {
    size_t len;
    size_t cap;
    char buf[0];
} BufHdr;

#define buf__hdr(b) ((BufHdr*)((char*)(b) - offsetof(BufHdr, buf)))
#define buf__fits(b, n) ((buf_len(b) + n) <= buf_cap(b))
#define buf__fit(b, n) (buf__fits((b),n) ? 0 : ((b) = buf__grow((b), buf_len(b) + n, sizeof(*(b)))))

#define buf_len(b) ((b) ? buf__hdr(b)->len : 0)
#define buf_cap(b) ((b) ? buf__hdr(b)->cap : 0)
#define buf_push(b, x) (buf__fit((b), 1), (b)[buf__hdr(b)->len++] = (x))
#define buf_free(b) ((b) ? (free(buf__hdr(b)), (b) = NULL) : 0)

void *buf__grow(const void *buf, size_t new_len, size_t elem_size) {
    size_t new_cap = MAX(1 + 2*buf_cap(buf), new_len);
    assert(new_len <= new_cap);
    size_t new_size = offsetof(BufHdr, buf) + (new_cap * elem_size);

    BufHdr *new_hdr;
    if (buf) {
        new_hdr = (BufHdr*) xrealloc(buf__hdr(buf), new_size);
    } else {
        new_hdr = (BufHdr*) xmalloc(new_size);
        new_hdr->len = 0;
    }
    new_hdr->cap = new_cap;

    return new_hdr->buf;
}

void buf_test() {
    int *buf = NULL;
    assert(buf_len(buf) == 0);
    assert(buf_cap(buf) == 0);

    enum { N = 1024 };
    for (int i = 0; i < N; i++) {
        buf_push(buf, i);
    }

    for (int i = 0; i < buf_len(buf); i++) {
        assert(i == buf[i]);
    }

    buf_free(buf);
    assert(buf == NULL);
    assert(buf_len(buf) == 0);
    assert(buf_cap(buf) == 0);
}

typedef struct {
    size_t len;
    const char *str;
} InternStr;

static InternStr *interns;

const char *str_intern_range(const char *start, const char *end) {
    size_t len = end - start; 
    for (size_t i = 0; i < buf_len(interns); i++) {
        if (interns[i].len == len && strncmp(interns[i].str, start, len) == 0) {
            return interns[i].str;
        }
    }

    char *str = xmalloc(len + 1);
    memcpy(str, start, len);
    str[len] = 0;

    buf_push(interns, ((InternStr){len, str}));

    return str;
}

const char *str_intern(const char *str) {
    return str_intern_range(str, str + strlen(str));
}

void str_intern_test() {
    char x[] = "hello";
    char y[] = "hello";
    assert(x != y);

    const char *px = str_intern(x);
    const char *py = str_intern(y);
    assert(px == py);

    char z[] = "hello!";
    const char *pz = str_intern(z);

    assert(py != pz);
}

typedef enum {
    TOKEN_INT = 128,
    TOKEN_NAME,
} TokenKind;


const char *token_kind_name(TokenKind kind) {
    static char buf[256];
    switch (kind) {
    case TOKEN_INT:
        sprintf(buf, "integer");
        break;
    case TOKEN_NAME:
        sprintf(buf, "name");
        break;
    default:
        if (kind < 128 && isprint(kind)) {
            sprintf(buf, "'%c'", kind);
        } else {
            sprintf(buf, "<ASCII %d>", kind);
        }
        break;
    }

    return buf;
}

typedef struct {
    TokenKind kind;
    const char *start;
    const char *end;

    union {
        u64 intval;
        const char *name;
    };
} Token;

Token token;
const char *stream;

const char *keyword_if;
const char *keyword_for;
const char *keyword_while;

void init_keywords() {
    keyword_if = str_intern("if");
    keyword_for = str_intern("for");
    keyword_while = str_intern("while");
}

void next_token() {
    token.start = stream;
    switch (*stream) {
    case '0' ... '9': {
        u64 val = 0;
        while (isdigit(*stream)) {
            val *= 10;
            val += *stream - '0';
            stream++;
        }
        token.kind = TOKEN_INT;
        token.intval = val;
    } break;
    case 'A' ... 'Z':
    case 'a' ... 'z':
    case '_': {
        while (isalnum(*stream) || *stream == '_') stream++;
        token.kind = TOKEN_NAME;
        token.name = str_intern_range(token.start, stream);
    } break;
    default:
        token.kind = *stream++;
        break;
    }
    token.end = stream;
}

void init_stream(const char *str) {
    stream = str;
    next_token();
}

void print_token(Token token) {
    printf("[TOKEN: ");
    if (token.kind < 128) {
        printf("'%c'", token.kind);
    } else {
        printf("%d", token.kind);
    }
    printf("] [LEXEME: %.*s]", (int)(token.end - token.start), token.start);
    switch (token.kind) {
    case TOKEN_INT:
        printf(" [VALUE: %llu]", token.intval);
        break;
    case TOKEN_NAME:
        printf(" [VALUE: %p -> \"%s\"]", token.name, token.name);
        break;
    }
    printf("\n");
}

static inline bool is_token(TokenKind kind) {
    return token.kind == kind;
}

static inline bool is_token_name(const char *name) {
    return token.kind == TOKEN_NAME && token.name == name;
}

static inline bool match_token(TokenKind kind) {
    if (is_token(kind)) {
        next_token();
        return true;
    }
    return false;
}

static inline bool expect_token(TokenKind kind) {
    if (is_token(kind)) {
        next_token();
        return true;
    }
    fatal("expected token %s: %s", token_kind_name(token.kind), token_kind_name(kind));
    return false;
}


void lex_test() {
    const char *source = "XYZ+(XYZ)12345+994";
    stream = source;
    next_token();

    while (token.kind) {
        print_token(token);
        next_token();
    }
}

/*
 *
 * expr3 = INT | '(' expr ')'
 * expr2 = [-]expr3
 * expr1 = expr2 ([*] expr2)*
 * expr0 = expr1 ([+-] expr1)*
 * expr = expr0
 *
 * */

int parse_expr();

int parse_expr3() {
    int val;

    if (is_token(TOKEN_INT)) {
        val = token.intval;
        next_token();
    } else if (match_token('(')) {
        val = parse_expr();
        expect_token(')');
    } else {
        fatal("expected integer or '(', got %s\n", token_kind_name(token.kind));
    }

    return val;
}

int parse_expr2() {
    if (match_token('-')) {
        return -parse_expr2();
    } 

    return parse_expr3();
}

int parse_expr1() {
    int val = parse_expr2();

    while (is_token('/') || is_token('*')) {
        char op = token.kind;
        next_token();
        int rval = parse_expr2();

        if (op == '/') {
            val /= rval;
        } else {
            assert(op == '*');
            val *= rval;
        }
    }

    return val;
}

int parse_expr0() {
    int val = parse_expr1();
    while (is_token('+') || is_token('-')) {
        char op = token.kind;
        next_token();
        int rval = parse_expr1();

        if (op == '+') {
            val += rval;
        } else {
            assert(op == '-');
            val -= rval;
        }
    }

    return val;
}

int parse_expr() {
    return parse_expr0();
}

void test_parse_expr(const char *expr) {
    init_stream(expr);
    int res = parse_expr();
    printf(expr);
    printf(" = %d\n", res);
}

void parse_test() {
    test_parse_expr("1+1");
    test_parse_expr("4*(3+1)");
}

int main(int argc, char **argv) {
    buf_test();
    str_intern_test();
    //lex_test();
    parse_test();

    return 0;
}
