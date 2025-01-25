#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <ctype.h>

#define MAX(x, y) (((x) >= (y)) ? (x) : (y))

typedef unsigned long long int u64;
typedef long long int s64;

typedef struct {
    size_t len;
    size_t cap;
    char buf[0];
} BufHdr;

#define buf__hdr(b) ((BufHdr*)((char*)(b) - offsetof(BufHdr, buf)))
#define buf__fits(b, n) ((buf_len(b) + n) <= buf_cap(b))
#define buf__fit(b, n) (buf__fits(b,n) ? 0 : ((b) = buf__grow(b, buf_len(b) + n, sizeof(*(b)))))

#define buf_len(b) ((b) ? buf__hdr(b)->len : 0)
#define buf_cap(b) ((b) ? buf__hdr(b)->cap : 0)
#define buf_push(b, x) (buf__fit(b, 1), (b)[buf_len(b)] = (x), buf__hdr(b)->len++)
#define buf_free(b) ((b) ? free(buf__hdr(b)), ((b) = NULL) : 0)

void *buf__grow(const void *buf, size_t new_len, size_t elem_size) {
    size_t new_cap = MAX(1 + 2*buf_cap(buf), new_len);
    assert(new_len <= new_cap);
    size_t new_size = offsetof(BufHdr, buf) + (new_cap * elem_size);

    BufHdr *new_hdr;
    if (buf) {
        new_hdr = (BufHdr*) realloc(buf__hdr(buf), new_size);
    } else {
        new_hdr = (BufHdr*) malloc(new_size);
        new_hdr->len = 0;
    }
    new_hdr->cap = new_cap;

    return new_hdr->buf;
}

void buf_test() {
    int *buf = NULL;

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

typedef enum {
    TOKEN_INT = 128,
    TOKEN_NAME,
} TokenKind;

typedef struct {
    TokenKind kind;
    const char *start;
    const char *end;

    union {
        u64 intval;
    };
} Token;

Token token;

const char *stream = "hi";

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
        const char *start = stream++;
        while (isalnum(*stream) || *stream == '_') {
            stream++;
        }
        token.kind = TOKEN_NAME;
    } break;
    default:
        token.kind = *stream++;
        break;
    }
    token.end = stream;
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
    }
    printf("\n");
}

void lex_test() {
    const char *source = "+()12345+994";
    stream = source;
    next_token();

    while (token.kind) {
        print_token(token);
        next_token();
    }
}

int main(int argc, char **argv) {
    buf_test();
    lex_test();

    return 0;
}
