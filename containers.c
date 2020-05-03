#include <stdlib.h>
#include <string.h>

#include "ccatd.h"

// Vector

struct Vector {
    int len;
    int cap;
    void **data;
};

Vec *vec_new() {
    Vec *vec = calloc(1, sizeof(Vec));
    vec->len = 0;
    vec->cap = 8;
    vec->data = calloc(8, sizeof(Node*));
    return vec;
}

int vec_len(Vec* vec) {
    return vec->len;
}

void vec_push(Vec *vec, void *node) {
    if (vec->len == vec->cap) {
        vec->cap *= 2;
        vec->data = realloc(vec->data, vec->cap * sizeof(void*));
    }
    vec->data[vec->len++] = node;
}

void *vec_pop(Vec *vec) {
    if (vec->len == 0)
        return NULL;

    void * ptr = vec->data[vec->len-1];
    vec->data[vec->len--] = NULL;
    return ptr;
}

void *vec_at(Vec *vec, int idx) {
    if (idx < 0 || vec->len <= idx)
        return NULL;
    return vec->data[idx];
}

// StringBuilder

struct StringBuilder {
    int len; int cap;
    char *data;
};

StringBuilder *strbld_new() {
    StringBuilder *sb = calloc(1, sizeof(StringBuilder));
    sb->len = 0;
    sb->cap = 8;
    sb->data = calloc(sb->cap, sizeof(char));
    return sb;
}

char *strbld_build(StringBuilder *sb) {
    char *ptr = calloc(sb->len+1, sizeof(char));
    strncpy(ptr, sb->data, sb->len);
    return ptr;
}

void strbld_append(StringBuilder *sb, char ch) {
    if (sb->len == sb->cap) {
        sb->cap *= 2;
        sb->data = realloc(sb->data, sb->cap * sizeof(char));
    }
    sb->data[sb->len++] = ch;
}

void strbld_append_str(StringBuilder *sb, char *ch) {
    while (*ch) strbld_append(sb, *(ch++));
}
